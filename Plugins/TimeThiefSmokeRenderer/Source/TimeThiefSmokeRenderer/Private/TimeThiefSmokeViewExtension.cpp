#include "TimeThiefSmokeViewExtension.h"

#include "PixelShaderUtils.h"
#include "HAL/IConsoleManager.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderCore.h"
#include "RenderTargetPool.h"
#include "RHITypes.h"
#include "SceneView.h"
#include "ScreenPass.h"
#include "TimeThiefSmokeShaders.h"

DEFINE_LOG_CATEGORY_STATIC(LogTimeThiefSmokeRenderer, Log, All);

namespace
{
	constexpr uint32 SmokeThreadGroupSize = 4;
	constexpr int32 MaxVortexParticleCount = 128;
	constexpr int32 MaxShaderEventCount = 128;
	constexpr int32 MaxDebugEventCount = 128;
	constexpr int32 MaxSimulationBulletEventCount = 32;
	constexpr int32 MaxSimulationExplosionEventCount = 8;
	constexpr int32 MaxSimulationActorEventCount = 32;
	constexpr int32 MaxSimulationForceEventCount = 32;
	constexpr float ObstacleMaskBlendDuration = 0.25f;
	constexpr int32 MultigridMaxLevelCount = 4;
	constexpr int32 MultigridCycleCount = 2;
	constexpr int32 MultigridPreSmoothPassCount = 2;
	constexpr int32 MultigridPostSmoothPassCount = 2;
	constexpr int32 MultigridCoarsestSmoothPassCount = 12;
	constexpr float CompositeFullscreenAreaThreshold = 0.58f;
	constexpr int32 CompositeTileSize = 32;
	constexpr int32 MaxCompositeSmokeSlots = 8;
	constexpr float VortexSubstepIntervalSeconds = 1.0f / 10.0f;

	static TAutoConsoleVariable<int32> CVarTimeThiefSmokeDebugView(
		TEXT("r.TimeThiefSmoke.DebugView"),
		0,
		TEXT("Custom smoke debug view. 0=off, 1=density, 2=obstacle, 3=bullet fields, 4=reserved, 5=events, 6=warp, 7=active bricks, 8=divergence, 9=bounds, 10=analytic bullet."));

	static TAutoConsoleVariable<int32> CVarTimeThiefSmokeScissor(
		TEXT("r.TimeThiefSmoke.Scissor"),
		1,
		TEXT("Limits custom smoke composite draw calls to each smoke AABB screen rect when enabled."));

	static TAutoConsoleVariable<int32> CVarTimeThiefSmokeProfile(
		TEXT("r.TimeThiefSmoke.Profile"),
		0,
		TEXT("Logs custom smoke grid, active brick, fallback, pass, VRAM, and render step estimates. 1=once per 60 frames, 2=every frame."));

	static TAutoConsoleVariable<int32> CVarTimeThiefSmokeFastFilament(
		TEXT("r.TimeThiefSmoke.FastFilament"),
		1,
		TEXT("Uses the cheaper filament shading branch for custom smoke. 0=full, 1=fast."));

	static TAutoConsoleVariable<float> CVarTimeThiefSmokeSimulationHz(
		TEXT("r.TimeThiefSmoke.SimulationHz"),
		30.0f,
		TEXT("Caps custom smoke simulation update rate. <=0 simulates every rendered frame."));

	int32 MakeAxisGridSize(const float AxisExtent, const float MaxExtent, const int32 MaxAxisResolution)
	{
		if (MaxExtent <= UE_SMALL_NUMBER)
		{
			return MaxAxisResolution;
		}

		const float AxisRatio = FMath::Clamp(AxisExtent / MaxExtent, 0.0f, 1.0f);
		const int32 RawResolution = FMath::RoundToInt(static_cast<float>(MaxAxisResolution) * AxisRatio);
		const int32 AlignedResolution = FMath::DivideAndRoundUp(FMath::Max(RawResolution, 16), static_cast<int32>(SmokeThreadGroupSize)) * static_cast<int32>(SmokeThreadGroupSize);
		return FMath::Clamp(AlignedResolution, 16, MaxAxisResolution);
	}

	FIntVector MakeGridSize(const int32 Resolution, const FVector3f& BoundsExtent)
	{
		const int32 MaxAxisResolution = FMath::Clamp(Resolution, 16, 512);
		const FVector3f AbsExtent(FMath::Abs(BoundsExtent.X), FMath::Abs(BoundsExtent.Y), FMath::Abs(BoundsExtent.Z));
		const float MaxExtent = FMath::Max(FMath::Max(AbsExtent.X, AbsExtent.Y), AbsExtent.Z);

		if (MaxExtent <= UE_SMALL_NUMBER)
		{
			return FIntVector(MaxAxisResolution, MaxAxisResolution, MaxAxisResolution);
		}

		return FIntVector(
			MakeAxisGridSize(AbsExtent.X, MaxExtent, MaxAxisResolution),
			MakeAxisGridSize(AbsExtent.Y, MaxExtent, MaxAxisResolution),
			MakeAxisGridSize(AbsExtent.Z, MaxExtent, MaxAxisResolution));
	}

	FIntVector MakeGroupCount(const FIntVector& GridSize)
	{
		return FIntVector(
			FMath::DivideAndRoundUp(GridSize.X, static_cast<int32>(SmokeThreadGroupSize)),
			FMath::DivideAndRoundUp(GridSize.Y, static_cast<int32>(SmokeThreadGroupSize)),
			FMath::DivideAndRoundUp(GridSize.Z, static_cast<int32>(SmokeThreadGroupSize)));
	}

	uint64 GetVoxelCount(const FIntVector& GridSize)
	{
		return static_cast<uint64>(FMath::Max(GridSize.X, 1)) *
			static_cast<uint64>(FMath::Max(GridSize.Y, 1)) *
			static_cast<uint64>(FMath::Max(GridSize.Z, 1));
	}

	uint64 EstimateTextureBytes(const FIntVector& GridSize, const uint64 BytesPerVoxel)
	{
		return GetVoxelCount(GridSize) * BytesPerVoxel;
	}

	bool ShouldUseSparseComposite(const FIntVector& BrickGridSize, const uint32 ActiveBrickCount)
	{
		if (ActiveBrickCount == 0u)
		{
			return true;
		}

		const uint64 TotalBrickCount = GetVoxelCount(BrickGridSize);
		return static_cast<uint64>(ActiveBrickCount) * 10u <= TotalBrickCount * 6u;
	}

	bool ShouldUseFullscreenComposite(const FIntRect& SmokeRect, const FIntRect& ViewRect)
	{
		const int64 ViewArea = static_cast<int64>(FMath::Max(0, ViewRect.Width())) * static_cast<int64>(FMath::Max(0, ViewRect.Height()));
		const int64 SmokeArea = static_cast<int64>(FMath::Max(0, SmokeRect.Width())) * static_cast<int64>(FMath::Max(0, SmokeRect.Height()));
		return ViewArea <= 0 || static_cast<float>(SmokeArea) >= static_cast<float>(ViewArea) * CompositeFullscreenAreaThreshold;
	}

	FVector3f MakeCellSize(const FTimeThiefSmokeRendererVolume& Volume, const FIntVector& GridSize)
	{
		const FVector3f GridSizeFloat(
			static_cast<float>(FMath::Max(GridSize.X, 1)),
			static_cast<float>(FMath::Max(GridSize.Y, 1)),
			static_cast<float>(FMath::Max(GridSize.Z, 1)));
		return (FVector3f(Volume.BoundsExtent) * 2.0f) / GridSizeFloat;
	}

	FIntVector MakeCoarseGridSize(const FIntVector& FineGridSize)
	{
		return FIntVector(
			FMath::Max(4, FMath::DivideAndRoundUp(FineGridSize.X, 2)),
			FMath::Max(4, FMath::DivideAndRoundUp(FineGridSize.Y, 2)),
			FMath::Max(4, FMath::DivideAndRoundUp(FineGridSize.Z, 2)));
	}

	int32 EstimateMultigridPassCount(const FIntVector& GridSize)
	{
		FIntVector LevelGridSizes[MultigridMaxLevelCount];
		int32 LevelCount = 1;
		LevelGridSizes[0] = GridSize;
		for (int32 LevelIndex = 1; LevelIndex < MultigridMaxLevelCount; ++LevelIndex)
		{
			LevelGridSizes[LevelIndex] = MakeCoarseGridSize(LevelGridSizes[LevelIndex - 1]);
			++LevelCount;
			if (FMath::Max3(LevelGridSizes[LevelIndex].X, LevelGridSizes[LevelIndex].Y, LevelGridSizes[LevelIndex].Z) <= 8)
			{
				break;
			}
		}

		const int32 DownPasses = (LevelCount - 1) * (MultigridPreSmoothPassCount + 1 + 1 + 2);
		const int32 UpPasses = (LevelCount - 1) * (1 + MultigridPostSmoothPassCount);
		return MultigridCycleCount * (DownPasses + MultigridCoarsestSmoothPassCount + UpPasses);
	}

	FIntVector MakeBrickGridSize(const FIntVector& GridSize, const int32 BrickSize)
	{
		const int32 SafeBrickSize = FMath::Max(BrickSize, 1);
		return FIntVector(
			FMath::Max(1, FMath::DivideAndRoundUp(GridSize.X, SafeBrickSize)),
			FMath::Max(1, FMath::DivideAndRoundUp(GridSize.Y, SafeBrickSize)),
			FMath::Max(1, FMath::DivideAndRoundUp(GridSize.Z, SafeBrickSize)));
	}

	FIntVector MakeSparseAtlasBrickGridSize(const int32 MaxActiveBricks)
	{
		const int32 SafeMaxActiveBricks = FMath::Max(MaxActiveBricks, 1);
		const int32 Axis = FMath::Max(1, FMath::CeilToInt(FMath::Pow(static_cast<float>(SafeMaxActiveBricks), 1.0f / 3.0f)));
		const int32 SliceCount = FMath::Max(1, FMath::DivideAndRoundUp(SafeMaxActiveBricks, Axis * Axis));
		return FIntVector(Axis, Axis, SliceCount);
	}

	FIntVector MakeSparseAtlasGridSize(const FIntVector& AtlasBrickGridSize, const int32 BrickSize)
	{
		const int32 SafeBrickSize = FMath::Max(BrickSize, 1);
		return FIntVector(
			FMath::Max(1, AtlasBrickGridSize.X * SafeBrickSize),
			FMath::Max(1, AtlasBrickGridSize.Y * SafeBrickSize),
			FMath::Max(1, AtlasBrickGridSize.Z * SafeBrickSize));
	}

	FIntVector MakeStableGridSize(
		const FTimeThiefSmokeRendererVolume& Volume,
		int32& OutEffectiveResolution)
	{
		const int32 RequestedResolution = FMath::Clamp(Volume.Settings.SmokeGridResolution, 16, 512);
		OutEffectiveResolution = RequestedResolution;
		return MakeGridSize(RequestedResolution, Volume.BoundsExtent);
	}

	FRDGTextureRef CreateTransientScalarTexture(
		FRDGBuilder& GraphBuilder,
		const FIntVector& GridSize,
		const TCHAR* Name)
	{
		const FRDGTextureDesc Desc = FRDGTextureDesc::Create3D(
			GridSize,
			PF_R16F,
			FClearValueBinding::Black,
			TexCreate_ShaderResource | TexCreate_UAV);
		return GraphBuilder.CreateTexture(Desc, Name);
	}

	FRDGTextureRef CreateTransientUIntTexture(
		FRDGBuilder& GraphBuilder,
		const FIntVector& GridSize,
		const TCHAR* Name)
	{
		const FRDGTextureDesc Desc = FRDGTextureDesc::Create3D(
			GridSize,
			PF_R32_UINT,
			FClearValueBinding::Black,
			TexCreate_ShaderResource | TexCreate_UAV);
		return GraphBuilder.CreateTexture(Desc, Name);
	}

	FTimeThiefSmokeEventShaderData ToShaderEvent(const FTimeThiefSmokeRendererEvent& Event)
	{
		FTimeThiefSmokeEventShaderData ShaderEvent;
		const FVector3f Position(Event.Position);
		const FVector3f Direction = FVector3f(Event.Direction.GetSafeNormal());
		const FVector3f Extents(Event.Extents);

		ShaderEvent.PositionRadius = FVector4f(Position.X, Position.Y, Position.Z, Event.Radius);
		ShaderEvent.DirectionLength = FVector4f(Direction.X, Direction.Y, Direction.Z, Event.Length);
		ShaderEvent.ExtentsStrength = FVector4f(Extents.X, Extents.Y, Extents.Z, Event.Strength);
		ShaderEvent.Rotation = FVector4f(
			static_cast<float>(Event.Rotation.X),
			static_cast<float>(Event.Rotation.Y),
			static_cast<float>(Event.Rotation.Z),
			static_cast<float>(Event.Rotation.W));
		ShaderEvent.TypeShapeAgeSeed = FVector4f(
			static_cast<float>(Event.Type),
			static_cast<float>(Event.Shape),
			Event.Type == ETimeThiefSmokeRendererInteractionType::ActorPush ? Event.WarpBudget : Event.NormalizedAge,
			static_cast<float>(Event.Seed));
		ShaderEvent.PreviousPositionSpeed = FVector4f(Event.PreviousPosition.X, Event.PreviousPosition.Y, Event.PreviousPosition.Z, Event.Speed);
		return ShaderEvent;
	}

	float ComputeSmokeEventPriority(const FTimeThiefSmokeRendererEvent& Event)
	{
		const float Strength = FMath::Max(Event.Strength, 0.01f);
		const float AgeWeight = FMath::Max(1.0f - FMath::Clamp(Event.NormalizedAge, 0.0f, 1.0f), 0.05f);
		const float RadiusWeight = FMath::Clamp(Event.Radius / 200.0f, 0.5f, 4.0f);
		float TypeWeight = 1.0f;
		switch (Event.Type)
		{
		case ETimeThiefSmokeRendererInteractionType::ExplosionShock:
			TypeWeight = 1.4f;
			break;
		case ETimeThiefSmokeRendererInteractionType::ActorPush:
			TypeWeight = 1.2f;
			break;
		case ETimeThiefSmokeRendererInteractionType::PlumeSource:
			TypeWeight = 1.35f;
			break;
		case ETimeThiefSmokeRendererInteractionType::BulletWake:
			TypeWeight = 1.1f;
			break;
		default:
			break;
		}
		return Strength * AgeWeight * RadiusWeight * TypeWeight;
	}

	void SortAndClampSmokeEvents(TArray<FTimeThiefSmokeRendererEvent>& Events, const int32 MaxEventCount)
	{
		Events.Sort(
			[](const FTimeThiefSmokeRendererEvent& Left, const FTimeThiefSmokeRendererEvent& Right)
			{
				return ComputeSmokeEventPriority(Left) > ComputeSmokeEventPriority(Right);
			});
		const int32 ClampedMaxEventCount = FMath::Clamp(MaxEventCount, 0, MaxShaderEventCount);
		if (Events.Num() > ClampedMaxEventCount)
		{
			Events.SetNum(ClampedMaxEventCount, EAllowShrinking::No);
		}
	}

	void AddPrioritizedSmokeEvent(TArray<FTimeThiefSmokeRendererEvent>& Events, const FTimeThiefSmokeRendererEvent& Event, const int32 MaxEventCount)
	{
		if (MaxEventCount <= 0)
		{
			return;
		}

		if (Events.Num() < MaxEventCount)
		{
			Events.Add(Event);
			return;
		}

		int32 LowestPriorityIndex = INDEX_NONE;
		float LowestPriority = TNumericLimits<float>::Max();
		for (int32 EventIndex = 0; EventIndex < Events.Num(); ++EventIndex)
		{
			const float Priority = ComputeSmokeEventPriority(Events[EventIndex]);
			if (Priority < LowestPriority)
			{
				LowestPriority = Priority;
				LowestPriorityIndex = EventIndex;
			}
		}

		const float NewPriority = ComputeSmokeEventPriority(Event);
		if (LowestPriorityIndex != INDEX_NONE && NewPriority > LowestPriority)
		{
			Events[LowestPriorityIndex] = Event;
		}
	}

	FRDGBufferRef CreateSmokeEventBuffer(
		FRDGBuilder& GraphBuilder,
		const TArray<FTimeThiefSmokeRendererEvent>& Events,
		int32& OutEventCount,
		const TCHAR* Name)
	{
		OutEventCount = FMath::Min(Events.Num(), MaxShaderEventCount);
		TArray<FTimeThiefSmokeEventShaderData> ShaderEvents;
		ShaderEvents.Reserve(FMath::Max(OutEventCount, 1));
		for (int32 EventIndex = 0; EventIndex < OutEventCount; ++EventIndex)
		{
			ShaderEvents.Add(ToShaderEvent(Events[EventIndex]));
		}
		if (ShaderEvents.IsEmpty())
		{
			ShaderEvents.AddDefaulted();
		}

		FRDGBufferRef EventBuffer = GraphBuilder.CreateBuffer(
			FRDGBufferDesc::CreateStructuredDesc(sizeof(FTimeThiefSmokeEventShaderData), ShaderEvents.Num()),
			Name);
		GraphBuilder.QueueBufferUpload(EventBuffer, ShaderEvents.GetData(), ShaderEvents.Num() * sizeof(FTimeThiefSmokeEventShaderData));
		return EventBuffer;
	}

	FRDGBufferRef CreatePrioritizedSmokeEventBuffer(
		FRDGBuilder& GraphBuilder,
		const TArray<FTimeThiefSmokeRendererEvent>& Events,
		const int32 MaxEventCount,
		int32& OutEventCount,
		const TCHAR* Name)
	{
		TArray<FTimeThiefSmokeRendererEvent> PrioritizedEvents = Events;
		SortAndClampSmokeEvents(PrioritizedEvents, MaxEventCount);
		return CreateSmokeEventBuffer(GraphBuilder, PrioritizedEvents, OutEventCount, Name);
	}

	FRDGBufferRef CreateTypedSmokeEventBuffer(
		FRDGBuilder& GraphBuilder,
		const TArray<FTimeThiefSmokeRendererEvent>& Events,
		const ETimeThiefSmokeRendererInteractionType Type,
		const int32 MaxEventCount,
		int32& OutEventCount,
		const TCHAR* Name)
	{
		TArray<FTimeThiefSmokeRendererEvent> TypedEvents;
		TypedEvents.Reserve(FMath::Min(Events.Num(), MaxEventCount));
		for (const FTimeThiefSmokeRendererEvent& Event : Events)
		{
			if (Event.Type == Type)
			{
				TypedEvents.Add(Event);
			}
		}
		SortAndClampSmokeEvents(TypedEvents, MaxEventCount);
		return CreateSmokeEventBuffer(GraphBuilder, TypedEvents, OutEventCount, Name);
	}

	FTransform ToDoubleTransform(const FTransform3f& Transform)
	{
		return FTransform(
			FQuat(Transform.GetRotation()),
			FVector(Transform.GetTranslation()),
			FVector(Transform.GetScale3D()));
	}

	FVector4f MakeMatrixRow(const FMatrix44f& Matrix, const int32 RowIndex)
	{
		return FVector4f(
			Matrix.M[RowIndex][0],
			Matrix.M[RowIndex][1],
			Matrix.M[RowIndex][2],
			Matrix.M[RowIndex][3]);
	}

	bool ComputeSmokeScreenRect(
		const FSceneView& View,
		const FTimeThiefSmokeRendererVolume& Volume,
		const FIntRect& ViewRect,
		FIntRect& OutRect)
	{
		const FVector BoundsExtent(Volume.RenderBoundsExtent);
		const FTransform LocalToWorld = ToDoubleTransform(Volume.LocalToWorld);
		const FMatrix ViewProjection = View.ViewMatrices.GetViewProjectionMatrix();
		bool bCrossesNearPlane = false;
		bool bHasProjectedCorner = false;
		float MinX = TNumericLimits<float>::Max();
		float MinY = TNumericLimits<float>::Max();
		float MaxX = -TNumericLimits<float>::Max();
		float MaxY = -TNumericLimits<float>::Max();

		for (int32 XSign = -1; XSign <= 1; XSign += 2)
		{
			for (int32 YSign = -1; YSign <= 1; YSign += 2)
			{
				for (int32 ZSign = -1; ZSign <= 1; ZSign += 2)
				{
					const FVector LocalCorner(
						BoundsExtent.X * static_cast<double>(XSign),
						BoundsExtent.Y * static_cast<double>(YSign),
						BoundsExtent.Z * static_cast<double>(ZSign));
					const FVector WorldCorner = LocalToWorld.TransformPosition(LocalCorner);
					const FVector4 Clip = ViewProjection.TransformFVector4(FVector4(WorldCorner, 1.0));
					if (Clip.W <= UE_SMALL_NUMBER)
					{
						bCrossesNearPlane = true;
						continue;
					}

					const float InvW = 1.0f / static_cast<float>(Clip.W);
					const float NdcX = static_cast<float>(Clip.X) * InvW;
					const float NdcY = static_cast<float>(Clip.Y) * InvW;
					const float ScreenX = static_cast<float>(ViewRect.Min.X) + (NdcX * 0.5f + 0.5f) * static_cast<float>(ViewRect.Width());
					const float ScreenY = static_cast<float>(ViewRect.Min.Y) + (0.5f - NdcY * 0.5f) * static_cast<float>(ViewRect.Height());
					MinX = FMath::Min(MinX, ScreenX);
					MinY = FMath::Min(MinY, ScreenY);
					MaxX = FMath::Max(MaxX, ScreenX);
					MaxY = FMath::Max(MaxY, ScreenY);
					bHasProjectedCorner = true;
				}
			}
		}

		if (bCrossesNearPlane)
		{
			OutRect = ViewRect;
			return true;
		}

		if (!bHasProjectedCorner)
		{
			return false;
		}

		constexpr int32 Padding = 12;
		const int32 RectMinX = FMath::Clamp(FMath::FloorToInt(MinX) - Padding, ViewRect.Min.X, ViewRect.Max.X);
		const int32 RectMinY = FMath::Clamp(FMath::FloorToInt(MinY) - Padding, ViewRect.Min.Y, ViewRect.Max.Y);
		const int32 RectMaxX = FMath::Clamp(FMath::CeilToInt(MaxX) + Padding, ViewRect.Min.X, ViewRect.Max.X);
		const int32 RectMaxY = FMath::Clamp(FMath::CeilToInt(MaxY) + Padding, ViewRect.Min.Y, ViewRect.Max.Y);
		OutRect = FIntRect(RectMinX, RectMinY, RectMaxX, RectMaxY);
		return OutRect.Width() > 0 && OutRect.Height() > 0;
	}
}

FTimeThiefSmokeViewExtension::FTimeThiefSmokeViewExtension(const FAutoRegister& AutoRegister)
	: FSceneViewExtensionBase(AutoRegister)
{
}

void FTimeThiefSmokeViewExtension::SubmitFrame_RenderThread(FTimeThiefSmokeRendererFrame&& Frame)
{
	check(IsInRenderingThread());

	LastFrameDeltaSeconds = FMath::Clamp(Frame.DeltaSeconds, 0.0f, 1.0f / 15.0f);

	TSet<int32> ActiveSmokeIds;
	for (const FTimeThiefSmokeRendererVolume& Volume : Frame.Volumes)
	{
		if (Volume.SmokeId == INDEX_NONE)
		{
			continue;
		}

		ActiveSmokeIds.Add(Volume.SmokeId);
		FRenderSmokeState& State = SmokeStates.FindOrAdd(Volume.SmokeId);
		const bool bSparseBackend = Volume.Settings.SimulationBackend == ETimeThiefSmokeSimulationBackend::SparseMac;
		if (!bSparseBackend)
		{
			State.bWarnedBrickBudgetOverflow = false;
			State.bForceDenseComposite = false;
		}
		if (bSparseBackend && State.bActiveBrickCountReadbackPending && State.ActiveBrickCountReadback.IsValid() && State.ActiveBrickCountReadback->IsReady())
		{
			const uint32* ActiveBrickCount = static_cast<const uint32*>(State.ActiveBrickCountReadback->Lock(sizeof(uint32)));
			State.LastActiveBrickCount = ActiveBrickCount ? ActiveBrickCount[0] : 0u;
			State.ActiveBrickCountReadback->Unlock();
			State.bActiveBrickCountReadbackPending = false;

			const int32 MaxActiveSmokeBricks = FMath::Max(Volume.Settings.MaxActiveSmokeBricks, 1);
			if (State.LastActiveBrickCount > static_cast<uint32>(MaxActiveSmokeBricks))
			{
				const bool bFallbackChanged = !State.bForceDenseComposite;
				State.bForceDenseComposite = true;
				if (!State.bWarnedBrickBudgetOverflow || bFallbackChanged)
				{
					UE_LOG(
						LogTimeThiefSmokeRenderer,
						Warning,
						TEXT("SmokeId %d active brick count %u exceeded sparse brick budget %d; using dense composite fallback while keeping simulation resolution stable."),
						Volume.SmokeId,
						State.LastActiveBrickCount,
						MaxActiveSmokeBricks);
				}
				State.bWarnedBrickBudgetOverflow = true;
			}
			else if (State.bForceDenseComposite && State.LastActiveBrickCount < static_cast<uint32>(FMath::Max(1, MaxActiveSmokeBricks / 2)))
			{
				State.bForceDenseComposite = false;
				State.bWarnedBrickBudgetOverflow = false;
			}
			else if (!State.bForceDenseComposite)
			{
				State.bWarnedBrickBudgetOverflow = false;
			}
		}
		int32 EffectiveResolution = 0;
		const FIntVector NewGridSize = MakeStableGridSize(Volume, EffectiveResolution);
		if (!bSparseBackend)
		{
			State.bWarnedBrickBudgetOverflow = false;
		}
		State.EffectiveSmokeGridResolution = EffectiveResolution;
		if (State.AllocatedGridSize != NewGridSize)
		{
			State.AllocatedGridSize = NewGridSize;
			State.DensityTextures[0].SafeRelease();
			State.DensityTextures[1].SafeRelease();
			State.VelocityTextures[0].SafeRelease();
			State.VelocityTextures[1].SafeRelease();
			State.MacVelocityUTextures[0].SafeRelease();
			State.MacVelocityUTextures[1].SafeRelease();
			State.MacVelocityVTextures[0].SafeRelease();
			State.MacVelocityVTextures[1].SafeRelease();
			State.MacVelocityWTextures[0].SafeRelease();
			State.MacVelocityWTextures[1].SafeRelease();
			State.PressureTextures[0].SafeRelease();
			State.PressureTextures[1].SafeRelease();
			State.DivergenceTexture.SafeRelease();
			State.BulletCutoutTextures[0].SafeRelease();
			State.BulletCutoutTextures[1].SafeRelease();
			State.BulletSinkTextures[0].SafeRelease();
			State.BulletSinkTextures[1].SafeRelease();
			State.WarpTextures[0].SafeRelease();
			State.WarpTextures[1].SafeRelease();
			State.ObstacleTexture.SafeRelease();
			State.BrickOccupancyTexture.SafeRelease();
			State.SparseFieldAtlasTexture.SafeRelease();
			State.CurlTexture.SafeRelease();
			State.VortexParticleBuffers[0].SafeRelease();
			State.VortexParticleBuffers[1].SafeRelease();
			State.CurrentDensityIndex = 0;
			State.CurrentVelocityIndex = 0;
			State.CurrentBulletFieldIndex = 0;
			State.CurrentWarpIndex = 0;
			State.CurrentVortexParticleIndex = 0;
			State.AnalyticBulletEvents.Reset();
			State.AllocatedObstacleResolution = 0;
			State.AllocatedBrickGridSize = FIntVector::ZeroValue;
			State.AllocatedSparseAtlasBrickGridSize = FIntVector::ZeroValue;
			State.AllocatedSparseAtlasGridSize = FIntVector::ZeroValue;
			State.UploadedObstacleMaskRevision = MAX_uint32;
			State.TargetObstacleMaskRevision = MAX_uint32;
			State.ObstacleMaskBlendAge = 1.0f;
			State.LastSimulatedFrame = MAX_uint32;
			State.AllocatedVortexParticleCount = 0;
			State.AccumulatedSimulationDeltaSeconds = 0.0f;
			State.AccumulatedVortexDeltaSeconds = 0.0f;
			State.WarpDecayBudgetSeconds = 0.0f;
			State.bVortexParticlesNeedUpload = true;
			State.bWarpClearPending = false;
			State.bUseSparseSimulationMaskThisFrame = false;
			State.bNeedsInit = true;
		}
		State.Volume = Volume;
	}

	for (auto It = SmokeStates.CreateIterator(); It; ++It)
	{
		if (!ActiveSmokeIds.Contains(It.Key()))
		{
			It.RemoveCurrent();
		}
		else
		{
			FRenderSmokeState& State = It.Value();
			State.LastDebugEvents.Reset();
			const float BulletLife = FMath::Max(State.Volume.Settings.BulletWakeMaxVisibleLife, 0.001f);
			const float BulletAgeStep = FMath::Max(LastFrameDeltaSeconds, 0.0f) / BulletLife;
			for (FTimeThiefSmokeRendererEvent& Event : State.AnalyticBulletEvents)
			{
				Event.NormalizedAge = FMath::Clamp(Event.NormalizedAge + BulletAgeStep, 0.0f, 1.0f);
			}
			State.AnalyticBulletEvents.RemoveAll(
				[](const FTimeThiefSmokeRendererEvent& Event)
				{
					return Event.NormalizedAge >= 1.0f;
				});
		}
	}

	for (const FTimeThiefSmokeRendererEvent& Event : Frame.Events)
	{
		if (FRenderSmokeState* State = SmokeStates.Find(Event.SmokeId))
		{
			const int32 MaxEvents = FMath::Clamp(State->Volume.Settings.MaxGPUEventsPerSmokePerFrame, 0, MaxShaderEventCount);
			if (State->LastDebugEvents.Num() < MaxDebugEventCount)
			{
				State->LastDebugEvents.Add(Event);
			}
			AddPrioritizedSmokeEvent(State->PendingEvents, Event, MaxEvents);

			if (Event.Type == ETimeThiefSmokeRendererInteractionType::BulletWake)
			{
				while (State->AnalyticBulletEvents.Num() >= MaxDebugEventCount)
				{
					State->AnalyticBulletEvents.RemoveAt(0, 1, EAllowShrinking::No);
				}

				FTimeThiefSmokeRendererEvent AnalyticEvent = Event;
				AnalyticEvent.NormalizedAge = 0.0f;
				State->AnalyticBulletEvents.Add(AnalyticEvent);
			}
		}
	}
}

void FTimeThiefSmokeViewExtension::Clear_RenderThread()
{
	check(IsInRenderingThread());
	SmokeStates.Reset();
}

void FTimeThiefSmokeViewExtension::PreRenderViewFamily_RenderThread(
	FRDGBuilder& GraphBuilder,
	FSceneViewFamily& ViewFamily)
{
	for (TPair<int32, FRenderSmokeState>& SmokePair : SmokeStates)
	{
		FRenderSmokeState& State = SmokePair.Value;
		if (State.LastSimulatedFrame == GFrameNumberRenderThread)
		{
			continue;
		}

		const float FrameDeltaSeconds = FMath::Clamp(LastFrameDeltaSeconds, 0.0f, 0.1f);
		const float SimulationHz = CVarTimeThiefSmokeSimulationHz.GetValueOnRenderThread();
		const float SimulationInterval = SimulationHz > 0.0f ? 1.0f / SimulationHz : 0.0f;
		State.AccumulatedSimulationDeltaSeconds = FMath::Min(State.AccumulatedSimulationDeltaSeconds + FrameDeltaSeconds, 0.1f);
		if (!State.bNeedsInit && SimulationInterval > 0.0f && State.AccumulatedSimulationDeltaSeconds < SimulationInterval)
		{
			State.LastSimulatedFrame = GFrameNumberRenderThread;
			continue;
		}

		const float SimulationDeltaSeconds = SimulationInterval > 0.0f
			? FMath::Max(State.AccumulatedSimulationDeltaSeconds, FrameDeltaSeconds)
			: FrameDeltaSeconds;
		State.AccumulatedSimulationDeltaSeconds = 0.0f;
		SimulateSmoke(GraphBuilder, State, SimulationDeltaSeconds);
		State.LastSimulatedFrame = GFrameNumberRenderThread;
		const int32 ProfileMode = CVarTimeThiefSmokeProfile.GetValueOnRenderThread();
		if (ProfileMode != 0)
		{
			const uint32 CurrentFrame = static_cast<uint32>(GFrameNumberRenderThread);
			if (ProfileMode > 1 || State.LastProfileLogFrame == 0 || CurrentFrame - State.LastProfileLogFrame >= 60)
			{
				const float EstimatedVRAMMB = static_cast<float>(State.LastEstimatedVRAMBytes) / (1024.0f * 1024.0f);
				const bool bDenseFallback = State.Volume.Settings.SimulationBackend == ETimeThiefSmokeSimulationBackend::SparseMac &&
					(State.bForceDenseComposite || !ShouldUseSparseComposite(State.AllocatedBrickGridSize, State.LastActiveBrickCount));
				UE_LOG(
					LogTimeThiefSmokeRenderer,
					Log,
					TEXT("SmokeProfile Id=%d Cluster=%d/%d Grid=%dx%dx%d ActiveBricks=%u DenseFallback=%d Passes~%d VRAM~%.2fMB RenderSteps=%d MaxSteps=%d StepScale=%.2f"),
					State.Volume.SmokeId,
					State.Volume.ClusterId,
					State.Volume.ClusterSourceCount,
					State.AllocatedGridSize.X,
					State.AllocatedGridSize.Y,
					State.AllocatedGridSize.Z,
					State.LastActiveBrickCount,
					bDenseFallback ? 1 : 0,
					State.LastProfilePassCount,
					EstimatedVRAMMB,
					FMath::Clamp(State.Volume.Settings.RenderStepCount, 8, 512),
					FMath::Clamp(State.Volume.Settings.RenderMaxStepCount, 16, 1024),
					FMath::Clamp(State.Volume.Settings.RenderStepVoxelScale, 0.1f, 4.0f));
				State.LastProfileLogFrame = CurrentFrame;
			}
		}
	}
}

void FTimeThiefSmokeViewExtension::SubscribeToPostProcessingPass(
	EPostProcessingPass Pass,
	const FSceneView& View,
	FAfterPassCallbackDelegateArray& InOutPassCallbacks,
	bool bIsPassEnabled)
{
	if (Pass == EPostProcessingPass::Tonemap)
	{
		InOutPassCallbacks.Add(FAfterPassCallbackDelegate::CreateRaw(this, &FTimeThiefSmokeViewExtension::CompositeSmoke_RenderThread));
	}
}

FScreenPassTexture FTimeThiefSmokeViewExtension::CompositeSmokeMulti_RenderThread(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const FPostProcessMaterialInputs& Inputs,
	const TArray<FRenderSmokeState*>& RenderStates,
	FScreenPassTexture CurrentSceneColor,
	const FMatrix44f& InvViewProjection,
	bool bAllowOverrideOutput)
{
	if (!CurrentSceneColor.IsValid() || RenderStates.Num() <= 1 || RenderStates.Num() > MaxCompositeSmokeSlots)
	{
		return CurrentSceneColor;
	}

	const bool bUseOverrideOutput = bAllowOverrideOutput &&
		Inputs.OverrideOutput.IsValid() &&
		Inputs.OverrideOutput.Texture != CurrentSceneColor.Texture;
	const FScreenPassViewInfo ViewInfo(View);
	const FIntRect ViewRect = bUseOverrideOutput ? Inputs.OverrideOutput.ViewRect : CurrentSceneColor.ViewRect;

	struct FCompositeVisibleSmoke
	{
		FRenderSmokeState* State = nullptr;
		FIntRect Rect;
	};

	TArray<FCompositeVisibleSmoke> VisibleSmokes;
	VisibleSmokes.Reserve(RenderStates.Num());
	for (FRenderSmokeState* State : RenderStates)
	{
		FIntRect SmokeRect = ViewRect;
		if (!ComputeSmokeScreenRect(View, State->Volume, ViewRect, SmokeRect))
		{
			continue;
		}

		FCompositeVisibleSmoke& VisibleSmoke = VisibleSmokes.AddDefaulted_GetRef();
		VisibleSmoke.State = State;
		VisibleSmoke.Rect = SmokeRect;
	}

	if (VisibleSmokes.Num() <= 1 || VisibleSmokes.Num() > MaxCompositeSmokeSlots)
	{
		return CurrentSceneColor;
	}

	FIntRect CompositeRect = VisibleSmokes[0].Rect;
	for (int32 SmokeIndex = 1; SmokeIndex < VisibleSmokes.Num(); ++SmokeIndex)
	{
		const FIntRect& SmokeRect = VisibleSmokes[SmokeIndex].Rect;
		CompositeRect.Min.X = FMath::Min(CompositeRect.Min.X, SmokeRect.Min.X);
		CompositeRect.Min.Y = FMath::Min(CompositeRect.Min.Y, SmokeRect.Min.Y);
		CompositeRect.Max.X = FMath::Max(CompositeRect.Max.X, SmokeRect.Max.X);
		CompositeRect.Max.Y = FMath::Max(CompositeRect.Max.Y, SmokeRect.Max.Y);
	}
	CompositeRect.Min.X = FMath::Clamp(CompositeRect.Min.X, ViewRect.Min.X, ViewRect.Max.X);
	CompositeRect.Min.Y = FMath::Clamp(CompositeRect.Min.Y, ViewRect.Min.Y, ViewRect.Max.Y);
	CompositeRect.Max.X = FMath::Clamp(CompositeRect.Max.X, ViewRect.Min.X, ViewRect.Max.X);
	CompositeRect.Max.Y = FMath::Clamp(CompositeRect.Max.Y, ViewRect.Min.Y, ViewRect.Max.Y);
	if (CompositeRect.Width() <= 0 || CompositeRect.Height() <= 0)
	{
		return CurrentSceneColor;
	}

	const bool bUseFullscreenComposite = ShouldUseFullscreenComposite(CompositeRect, ViewRect);
	const FIntRect DrawRect = bUseFullscreenComposite ? ViewRect : CompositeRect;
	const FIntPoint TileGridSize(
		FMath::Max(1, FMath::DivideAndRoundUp(DrawRect.Width(), CompositeTileSize)),
		FMath::Max(1, FMath::DivideAndRoundUp(DrawRect.Height(), CompositeTileSize)));
	const int32 TileCount = TileGridSize.X * TileGridSize.Y;

	TArray<int32> TileSmokeCounts;
	TileSmokeCounts.Init(0, TileCount);
	for (int32 SmokeSlot = 0; SmokeSlot < VisibleSmokes.Num(); ++SmokeSlot)
	{
		const FIntRect& SmokeRect = VisibleSmokes[SmokeSlot].Rect;
		const int32 MinTileX = FMath::Clamp((SmokeRect.Min.X - DrawRect.Min.X) / CompositeTileSize, 0, TileGridSize.X - 1);
		const int32 MinTileY = FMath::Clamp((SmokeRect.Min.Y - DrawRect.Min.Y) / CompositeTileSize, 0, TileGridSize.Y - 1);
		const int32 MaxTileX = FMath::Clamp((FMath::Max(SmokeRect.Max.X - 1, SmokeRect.Min.X) - DrawRect.Min.X) / CompositeTileSize, 0, TileGridSize.X - 1);
		const int32 MaxTileY = FMath::Clamp((FMath::Max(SmokeRect.Max.Y - 1, SmokeRect.Min.Y) - DrawRect.Min.Y) / CompositeTileSize, 0, TileGridSize.Y - 1);
		for (int32 TileY = MinTileY; TileY <= MaxTileY; ++TileY)
		{
			for (int32 TileX = MinTileX; TileX <= MaxTileX; ++TileX)
			{
				++TileSmokeCounts[TileY * TileGridSize.X + TileX];
			}
		}
	}

	TArray<FTimeThiefSmokeCompositeTileRangeShaderData> TileRanges;
	TArray<uint32> TileIndices;
	TileRanges.SetNum(TileCount);
	TArray<int32> TileWriteOffsets;
	TileWriteOffsets.SetNum(TileCount);
	int32 TotalTileSmokeIndexCount = 0;
	int32 OverlappedTileCount = 0;
	for (int32 TileIndex = 0; TileIndex < TileCount; ++TileIndex)
	{
		const int32 Count = TileSmokeCounts[TileIndex];
		OverlappedTileCount += Count > 1 ? 1 : 0;
		TileWriteOffsets[TileIndex] = TotalTileSmokeIndexCount;
		TileRanges[TileIndex].OffsetCount = FVector4f(static_cast<float>(TotalTileSmokeIndexCount), static_cast<float>(Count), 0.0f, 0.0f);
		TotalTileSmokeIndexCount += Count;
	}
	const float AverageTileSmokeCount = static_cast<float>(TotalTileSmokeIndexCount) / static_cast<float>(FMath::Max(TileCount, 1));
	const float OverlappedTileRatio = static_cast<float>(OverlappedTileCount) / static_cast<float>(FMath::Max(TileCount, 1));
	if (OverlappedTileCount == 0)
	{
		return CurrentSceneColor;
	}

	const FScreenPassRenderTarget Output = bUseOverrideOutput
		? Inputs.OverrideOutput
		: FScreenPassRenderTarget::CreateFromInput(GraphBuilder, CurrentSceneColor, ERenderTargetLoadAction::ELoad, TEXT("TimeThiefSmoke.CompositeMulti"));
	if (!Output.IsValid())
	{
		return CurrentSceneColor;
	}

	if (!bUseFullscreenComposite && Output.Texture != CurrentSceneColor.Texture)
	{
		AddDrawTexturePass(GraphBuilder, ViewInfo, CurrentSceneColor, Output);
	}

	if (TotalTileSmokeIndexCount > 0)
	{
		TileIndices.SetNumUninitialized(TotalTileSmokeIndexCount);
		for (int32 SmokeSlot = 0; SmokeSlot < VisibleSmokes.Num(); ++SmokeSlot)
		{
			const FIntRect& SmokeRect = VisibleSmokes[SmokeSlot].Rect;
			const int32 MinTileX = FMath::Clamp((SmokeRect.Min.X - DrawRect.Min.X) / CompositeTileSize, 0, TileGridSize.X - 1);
			const int32 MinTileY = FMath::Clamp((SmokeRect.Min.Y - DrawRect.Min.Y) / CompositeTileSize, 0, TileGridSize.Y - 1);
			const int32 MaxTileX = FMath::Clamp((FMath::Max(SmokeRect.Max.X - 1, SmokeRect.Min.X) - DrawRect.Min.X) / CompositeTileSize, 0, TileGridSize.X - 1);
			const int32 MaxTileY = FMath::Clamp((FMath::Max(SmokeRect.Max.Y - 1, SmokeRect.Min.Y) - DrawRect.Min.Y) / CompositeTileSize, 0, TileGridSize.Y - 1);
			for (int32 TileY = MinTileY; TileY <= MaxTileY; ++TileY)
			{
				for (int32 TileX = MinTileX; TileX <= MaxTileX; ++TileX)
				{
					const int32 TileIndex = TileY * TileGridSize.X + TileX;
					TileIndices[TileWriteOffsets[TileIndex]++] = static_cast<uint32>(SmokeSlot);
				}
			}
		}
	}
	else
	{
		TileIndices.Add(0u);
	}

	TArray<FTimeThiefSmokeCompositeDescriptorShaderData> Descriptors;
	TArray<FTimeThiefSmokeEventShaderData> PackedEvents;
	Descriptors.Reserve(VisibleSmokes.Num());
	PackedEvents.Reserve(VisibleSmokes.Num() * 8);
	const bool bUseFastFilament = CVarTimeThiefSmokeFastFilament.GetValueOnRenderThread() != 0;

	for (const FCompositeVisibleSmoke& Entry : VisibleSmokes)
	{
		const FRenderSmokeState& State = *Entry.State;
		const FMatrix44f LocalToWorld = State.Volume.LocalToWorld.ToMatrixWithScale();
		const FMatrix44f WorldToLocal = State.Volume.LocalToWorld.ToInverseMatrixWithScale();
		const bool bUseSparseComposite = State.Volume.Settings.SimulationBackend == ETimeThiefSmokeSimulationBackend::SparseMac &&
			!State.bForceDenseComposite &&
			ShouldUseSparseComposite(State.AllocatedBrickGridSize, State.LastActiveBrickCount);

		const int32 EventOffset = PackedEvents.Num();
		int32 AnalyticBulletEventCount = 0;
		for (const FTimeThiefSmokeRendererEvent& Event : State.AnalyticBulletEvents)
		{
			if (AnalyticBulletEventCount >= MaxDebugEventCount)
			{
				break;
			}

			PackedEvents.Add(ToShaderEvent(Event));
			++AnalyticBulletEventCount;
		}

		FTimeThiefSmokeCompositeDescriptorShaderData Descriptor;
		Descriptor.LocalToWorld0 = MakeMatrixRow(LocalToWorld, 0);
		Descriptor.LocalToWorld1 = MakeMatrixRow(LocalToWorld, 1);
		Descriptor.LocalToWorld2 = MakeMatrixRow(LocalToWorld, 2);
		Descriptor.LocalToWorld3 = MakeMatrixRow(LocalToWorld, 3);
		Descriptor.WorldToLocal0 = MakeMatrixRow(WorldToLocal, 0);
		Descriptor.WorldToLocal1 = MakeMatrixRow(WorldToLocal, 1);
		Descriptor.WorldToLocal2 = MakeMatrixRow(WorldToLocal, 2);
		Descriptor.WorldToLocal3 = MakeMatrixRow(WorldToLocal, 3);
		Descriptor.BoundsExtent_RenderStepVoxelScale = FVector4f(State.Volume.BoundsExtent.X, State.Volume.BoundsExtent.Y, State.Volume.BoundsExtent.Z, FMath::Clamp(State.Volume.Settings.RenderStepVoxelScale, 0.1f, 4.0f));
		Descriptor.RenderBoundsExtent_Extinction = FVector4f(State.Volume.RenderBoundsExtent.X, State.Volume.RenderBoundsExtent.Y, State.Volume.RenderBoundsExtent.Z, State.Volume.Settings.Extinction);
		Descriptor.ScatterNoise = FVector4f(State.Volume.Settings.ScatteringAlbedo, State.Volume.Settings.ScatteringAnisotropy, State.Volume.Settings.RenderNoiseScale, State.Volume.Settings.RenderNoiseStrength);
		Descriptor.NoiseFilamentA = FVector4f(State.Volume.Settings.RenderNoiseTimeScale, State.Volume.Settings.RenderFilamentScale, State.Volume.Settings.RenderFilamentStrength, State.Volume.Settings.RenderFilamentContrast);
		Descriptor.FilamentAge = FVector4f(State.Volume.Settings.RenderFilamentWarpStrength, State.Volume.AgeSeconds, State.Volume.DurationSeconds, State.Volume.Settings.SmokeFadeOutDuration);
		Descriptor.GridResolution_UseSparse = FVector4f(static_cast<float>(State.AllocatedGridSize.X), static_cast<float>(State.AllocatedGridSize.Y), static_cast<float>(State.AllocatedGridSize.Z), bUseSparseComposite ? 1.0f : 0.0f);
		Descriptor.BrickGridResolution_SmokeBrickSize = FVector4f(static_cast<float>(State.AllocatedBrickGridSize.X), static_cast<float>(State.AllocatedBrickGridSize.Y), static_cast<float>(State.AllocatedBrickGridSize.Z), static_cast<float>(FMath::Clamp(State.Volume.Settings.SmokeBrickSize, 4, 32)));
		Descriptor.SparseAtlasBrickGridResolution_MaxActive = FVector4f(static_cast<float>(State.AllocatedSparseAtlasBrickGridSize.X), static_cast<float>(State.AllocatedSparseAtlasBrickGridSize.Y), static_cast<float>(State.AllocatedSparseAtlasBrickGridSize.Z), static_cast<float>(FMath::Max(State.Volume.Settings.MaxActiveSmokeBricks, 1)));
		Descriptor.RenderSteps_Events = FVector4f(static_cast<float>(FMath::Clamp(State.Volume.Settings.RenderStepCount, 8, 512)), static_cast<float>(FMath::Clamp(State.Volume.Settings.RenderMaxStepCount, 16, 1024)), static_cast<float>(EventOffset), static_cast<float>(AnalyticBulletEventCount));
		Descriptor.AnalyticEvents = FVector4f(static_cast<float>(AnalyticBulletEventCount), State.Volume.NaturalBoundsExtent.X, State.Volume.NaturalBoundsExtent.Y, State.Volume.NaturalBoundsExtent.Z);
		Descriptor.RenderQuality = FVector4f(bUseFastFilament ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);
		Descriptors.Add(Descriptor);
	}

	if (PackedEvents.IsEmpty())
	{
		PackedEvents.AddDefaulted();
	}

	FRDGBufferRef DescriptorBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(FTimeThiefSmokeCompositeDescriptorShaderData), Descriptors.Num()), TEXT("TimeThiefSmoke.CompositeMultiDescriptors"));
	GraphBuilder.QueueBufferUpload(DescriptorBuffer, Descriptors.GetData(), Descriptors.Num() * sizeof(FTimeThiefSmokeCompositeDescriptorShaderData));
	FRDGBufferRef TileRangeBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(FTimeThiefSmokeCompositeTileRangeShaderData), TileRanges.Num()), TEXT("TimeThiefSmoke.CompositeMultiTileRanges"));
	GraphBuilder.QueueBufferUpload(TileRangeBuffer, TileRanges.GetData(), TileRanges.Num() * sizeof(FTimeThiefSmokeCompositeTileRangeShaderData));
	FRDGBufferRef TileIndexBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), TileIndices.Num()), TEXT("TimeThiefSmoke.CompositeMultiTileIndices"));
	GraphBuilder.QueueBufferUpload(TileIndexBuffer, TileIndices.GetData(), TileIndices.Num() * sizeof(uint32));
	FRDGBufferRef EventBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(FTimeThiefSmokeEventShaderData), PackedEvents.Num()), TEXT("TimeThiefSmoke.CompositeMultiEvents"));
	GraphBuilder.QueueBufferUpload(EventBuffer, PackedEvents.GetData(), PackedEvents.Num() * sizeof(FTimeThiefSmokeEventShaderData));

	TShaderMapRef<FTimeThiefSmokeCompositeMultiPS> PixelShader(GetGlobalShaderMap(View.FeatureLevel));
	FTimeThiefSmokeCompositeMultiPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeCompositeMultiPS::FParameters>();
	PassParameters->SceneColorTexture = CurrentSceneColor.Texture;
	PassParameters->SceneDepthTexture = Inputs.SceneTextures.SceneTextures->GetParameters()->SceneDepthTexture;
	PassParameters->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->VolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->CompositeSmokeDescriptors = GraphBuilder.CreateSRV(DescriptorBuffer);
	PassParameters->TileSmokeRanges = GraphBuilder.CreateSRV(TileRangeBuffer);
	PassParameters->TileSmokeIndices = GraphBuilder.CreateSRV(TileIndexBuffer);
	PassParameters->Events = GraphBuilder.CreateSRV(EventBuffer);

	const FVector2f SceneColorTextureExtent(static_cast<float>(FMath::Max(1, CurrentSceneColor.Texture->Desc.Extent.X)), static_cast<float>(FMath::Max(1, CurrentSceneColor.Texture->Desc.Extent.Y)));
	const FVector2f InputRectMin(static_cast<float>(CurrentSceneColor.ViewRect.Min.X), static_cast<float>(CurrentSceneColor.ViewRect.Min.Y));
	const FVector2f OutputRectMin(static_cast<float>(Output.ViewRect.Min.X), static_cast<float>(Output.ViewRect.Min.Y));
	const FVector2f InputToOutputScale(
		static_cast<float>(CurrentSceneColor.ViewRect.Width()) / static_cast<float>(FMath::Max(1, Output.ViewRect.Width())),
		static_cast<float>(CurrentSceneColor.ViewRect.Height()) / static_cast<float>(FMath::Max(1, Output.ViewRect.Height())));
	PassParameters->SceneColorUVScaleBias = FVector4f(
		InputToOutputScale.X / SceneColorTextureExtent.X,
		InputToOutputScale.Y / SceneColorTextureExtent.Y,
		(InputRectMin.X - OutputRectMin.X * InputToOutputScale.X) / SceneColorTextureExtent.X,
		(InputRectMin.Y - OutputRectMin.Y * InputToOutputScale.Y) / SceneColorTextureExtent.Y);
	PassParameters->ViewRect = Output.ViewRect;
	PassParameters->TileRectMin = DrawRect.Min;
	PassParameters->TileGridSize = TileGridSize;
	PassParameters->CompositeTileSize = CompositeTileSize;
	PassParameters->SmokeSlotCount = VisibleSmokes.Num();
	PassParameters->InvViewProjection = InvViewProjection;

	const auto RegisterMultiSmokeTextures = [&GraphBuilder, PassParameters](const int32 Slot, FRenderSmokeState& State)
	{
		FRDGTextureRef DensityTexture = GraphBuilder.RegisterExternalTexture(State.DensityTextures[State.CurrentDensityIndex]);
		FRDGTextureRef WarpTexture = GraphBuilder.RegisterExternalTexture(State.WarpTextures[State.CurrentWarpIndex]);
		FRDGTextureRef ObstacleTexture = GraphBuilder.RegisterExternalTexture(State.ObstacleTexture);
		FRDGTextureRef BulletCutoutTexture = GraphBuilder.RegisterExternalTexture(State.BulletCutoutTextures[State.CurrentBulletFieldIndex]);
		FRDGTextureRef BulletSinkTexture = GraphBuilder.RegisterExternalTexture(State.BulletSinkTextures[State.CurrentBulletFieldIndex]);
		FRDGTextureRef BrickOccupancyTexture = GraphBuilder.RegisterExternalTexture(State.BrickOccupancyTexture);
		FRDGTextureRef SparseFieldAtlasTexture = GraphBuilder.RegisterExternalTexture(State.SparseFieldAtlasTexture);

		switch (Slot)
		{
		case 0:
			PassParameters->DensityTexture0 = DensityTexture;
			PassParameters->WarpTexture0 = WarpTexture;
			PassParameters->ObstacleTexture0 = ObstacleTexture;
			PassParameters->BulletCutoutTexture0 = BulletCutoutTexture;
			PassParameters->BulletSinkTexture0 = BulletSinkTexture;
			PassParameters->BrickOccupancyTexture0 = BrickOccupancyTexture;
			PassParameters->SparseFieldAtlasTexture0 = SparseFieldAtlasTexture;
			break;
		case 1:
			PassParameters->DensityTexture1 = DensityTexture;
			PassParameters->WarpTexture1 = WarpTexture;
			PassParameters->ObstacleTexture1 = ObstacleTexture;
			PassParameters->BulletCutoutTexture1 = BulletCutoutTexture;
			PassParameters->BulletSinkTexture1 = BulletSinkTexture;
			PassParameters->BrickOccupancyTexture1 = BrickOccupancyTexture;
			PassParameters->SparseFieldAtlasTexture1 = SparseFieldAtlasTexture;
			break;
		case 2:
			PassParameters->DensityTexture2 = DensityTexture;
			PassParameters->WarpTexture2 = WarpTexture;
			PassParameters->ObstacleTexture2 = ObstacleTexture;
			PassParameters->BulletCutoutTexture2 = BulletCutoutTexture;
			PassParameters->BulletSinkTexture2 = BulletSinkTexture;
			PassParameters->BrickOccupancyTexture2 = BrickOccupancyTexture;
			PassParameters->SparseFieldAtlasTexture2 = SparseFieldAtlasTexture;
			break;
		case 3:
			PassParameters->DensityTexture3 = DensityTexture;
			PassParameters->WarpTexture3 = WarpTexture;
			PassParameters->ObstacleTexture3 = ObstacleTexture;
			PassParameters->BulletCutoutTexture3 = BulletCutoutTexture;
			PassParameters->BulletSinkTexture3 = BulletSinkTexture;
			PassParameters->BrickOccupancyTexture3 = BrickOccupancyTexture;
			PassParameters->SparseFieldAtlasTexture3 = SparseFieldAtlasTexture;
			break;
		case 4:
			PassParameters->DensityTexture4 = DensityTexture;
			PassParameters->WarpTexture4 = WarpTexture;
			PassParameters->ObstacleTexture4 = ObstacleTexture;
			PassParameters->BulletCutoutTexture4 = BulletCutoutTexture;
			PassParameters->BulletSinkTexture4 = BulletSinkTexture;
			PassParameters->BrickOccupancyTexture4 = BrickOccupancyTexture;
			PassParameters->SparseFieldAtlasTexture4 = SparseFieldAtlasTexture;
			break;
		case 5:
			PassParameters->DensityTexture5 = DensityTexture;
			PassParameters->WarpTexture5 = WarpTexture;
			PassParameters->ObstacleTexture5 = ObstacleTexture;
			PassParameters->BulletCutoutTexture5 = BulletCutoutTexture;
			PassParameters->BulletSinkTexture5 = BulletSinkTexture;
			PassParameters->BrickOccupancyTexture5 = BrickOccupancyTexture;
			PassParameters->SparseFieldAtlasTexture5 = SparseFieldAtlasTexture;
			break;
		case 6:
			PassParameters->DensityTexture6 = DensityTexture;
			PassParameters->WarpTexture6 = WarpTexture;
			PassParameters->ObstacleTexture6 = ObstacleTexture;
			PassParameters->BulletCutoutTexture6 = BulletCutoutTexture;
			PassParameters->BulletSinkTexture6 = BulletSinkTexture;
			PassParameters->BrickOccupancyTexture6 = BrickOccupancyTexture;
			PassParameters->SparseFieldAtlasTexture6 = SparseFieldAtlasTexture;
			break;
		default:
			PassParameters->DensityTexture7 = DensityTexture;
			PassParameters->WarpTexture7 = WarpTexture;
			PassParameters->ObstacleTexture7 = ObstacleTexture;
			PassParameters->BulletCutoutTexture7 = BulletCutoutTexture;
			PassParameters->BulletSinkTexture7 = BulletSinkTexture;
			PassParameters->BrickOccupancyTexture7 = BrickOccupancyTexture;
			PassParameters->SparseFieldAtlasTexture7 = SparseFieldAtlasTexture;
			break;
		}
	};

	for (int32 Slot = 0; Slot < MaxCompositeSmokeSlots; ++Slot)
	{
		FRenderSmokeState& State = *VisibleSmokes[FMath::Min(Slot, VisibleSmokes.Num() - 1)].State;
		RegisterMultiSmokeTextures(Slot, State);
	}
	PassParameters->RenderTargets[0] = Output.GetRenderTargetBinding();

	const int32 ProfileMode = CVarTimeThiefSmokeProfile.GetValueOnRenderThread();
	static uint32 LastCompositeProfileLogFrame = 0;
	if (ProfileMode != 0)
	{
		const uint32 CurrentFrame = static_cast<uint32>(GFrameCounterRenderThread);
		if (ProfileMode > 1 || LastCompositeProfileLogFrame == 0 || CurrentFrame - LastCompositeProfileLogFrame >= 60)
		{
			UE_LOG(LogTimeThiefSmokeRenderer, Log, TEXT("SmokeCompositeProfile Mode=Multi VisibleSmokes=%d Tiles=%dx%d DrawRect=%dx%d AvgTileSmokes=%.2f OverlapTiles=%d OverlapRatio=%.2f Batches=1"), VisibleSmokes.Num(), TileGridSize.X, TileGridSize.Y, DrawRect.Width(), DrawRect.Height(), AverageTileSmokeCount, OverlappedTileCount, OverlappedTileRatio);
			LastCompositeProfileLogFrame = CurrentFrame;
		}
	}

	AddDrawScreenPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.CompositeMulti Smokes=%d Tiles=%dx%d", VisibleSmokes.Num(), TileGridSize.X, TileGridSize.Y),
		ViewInfo,
		FScreenPassTextureViewport(Output.Texture, DrawRect),
		FScreenPassTextureViewport(CurrentSceneColor),
		PixelShader,
		PassParameters);

	return Output;
}

FScreenPassTexture FTimeThiefSmokeViewExtension::CompositeSmoke_RenderThread(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const FPostProcessMaterialInputs& Inputs)
{
	if (SmokeStates.IsEmpty())
	{
		return Inputs.ReturnUntouchedSceneColorForPostProcessing(GraphBuilder);
	}

	const FScreenPassTextureSlice SceneColorSlice = Inputs.GetInput(EPostProcessMaterialInput::SceneColor);
	FScreenPassTexture CurrentSceneColor(SceneColorSlice);
	if (!CurrentSceneColor.IsValid())
	{
		return Inputs.ReturnUntouchedSceneColorForPostProcessing(GraphBuilder);
	}
	const FRDGTextureRef OriginalSceneColorTexture = CurrentSceneColor.Texture;

	const FScreenPassViewInfo ViewInfo(View);
	const FMatrix44f InvViewProjection(View.ViewMatrices.GetInvViewProjectionMatrix());
	const int32 DebugMode = FMath::Clamp(CVarTimeThiefSmokeDebugView.GetValueOnRenderThread(), 0, 10);
	const bool bUseScissor = CVarTimeThiefSmokeScissor.GetValueOnRenderThread() != 0;

	TArray<FRenderSmokeState*> RenderStates;
	RenderStates.Reserve(SmokeStates.Num());
	for (TPair<int32, FRenderSmokeState>& SmokePair : SmokeStates)
	{
		FRenderSmokeState& State = SmokePair.Value;
		if (State.DensityTextures[State.CurrentDensityIndex].IsValid() &&
			State.WarpTextures[State.CurrentWarpIndex].IsValid() &&
			State.BulletCutoutTextures[State.CurrentBulletFieldIndex].IsValid() &&
			State.BulletSinkTextures[State.CurrentBulletFieldIndex].IsValid() &&
			State.DivergenceTexture.IsValid() &&
			State.BrickOccupancyTexture.IsValid() &&
			State.SparseFieldAtlasTexture.IsValid())
		{
			RenderStates.Add(&State);
		}
	}

	if (RenderStates.IsEmpty())
	{
		return Inputs.ReturnUntouchedSceneColorForPostProcessing(GraphBuilder);
	}

	const auto ReturnCurrentOrOverrideOutput = [&]() -> FScreenPassTexture
	{
		const bool bNoSmokePassEmitted =
			CurrentSceneColor.Texture == OriginalSceneColorTexture &&
			(!Inputs.OverrideOutput.IsValid() || Inputs.OverrideOutput.Texture != CurrentSceneColor.Texture);
		if (bNoSmokePassEmitted)
		{
			return Inputs.ReturnUntouchedSceneColorForPostProcessing(GraphBuilder);
		}
		if (Inputs.OverrideOutput.IsValid() && Inputs.OverrideOutput.Texture != CurrentSceneColor.Texture)
		{
			AddDrawTexturePass(GraphBuilder, ViewInfo, CurrentSceneColor, Inputs.OverrideOutput);
			return Inputs.OverrideOutput;
		}
		return CurrentSceneColor;
	};

	struct FCompositeCandidate
	{
		FRenderSmokeState* State = nullptr;
		FIntRect Rect;
		bool bValid = false;
	};

	TArray<FCompositeCandidate> Candidates;
	Candidates.SetNum(RenderStates.Num());
	bool bHasVisibleSmoke = false;
	for (int32 StateIndex = 0; StateIndex < RenderStates.Num(); ++StateIndex)
	{
		Candidates[StateIndex].State = RenderStates[StateIndex];
		Candidates[StateIndex].bValid = ComputeSmokeScreenRect(View, RenderStates[StateIndex]->Volume, CurrentSceneColor.ViewRect, Candidates[StateIndex].Rect);
		bHasVisibleSmoke |= Candidates[StateIndex].bValid;
	}
	if (!bHasVisibleSmoke)
	{
		return ReturnCurrentOrOverrideOutput();
	}

	TArray<uint8> bHandledByMulti;
	bHandledByMulti.Init(0, RenderStates.Num());
	if (DebugMode == 0 &&
		RenderStates.Num() > 1)
	{
		const auto RectsOverlap = [](const FIntRect& A, const FIntRect& B)
		{
			return A.Min.X < B.Max.X &&
				A.Max.X > B.Min.X &&
				A.Min.Y < B.Max.Y &&
				A.Max.Y > B.Min.Y;
		};

		TArray<uint8> bVisited;
		bVisited.Init(0, RenderStates.Num());
		for (int32 SeedIndex = 0; SeedIndex < Candidates.Num(); ++SeedIndex)
		{
			if (bVisited[SeedIndex] || !Candidates[SeedIndex].bValid)
			{
				continue;
			}

			TArray<int32> GroupIndices;
			GroupIndices.Reserve(MaxCompositeSmokeSlots);
			GroupIndices.Add(SeedIndex);
			bVisited[SeedIndex] = 1;

			for (int32 ReadIndex = 0; ReadIndex < GroupIndices.Num(); ++ReadIndex)
			{
				const int32 CurrentIndex = GroupIndices[ReadIndex];
				for (int32 CandidateIndex = SeedIndex + 1; CandidateIndex < Candidates.Num(); ++CandidateIndex)
				{
					if (bVisited[CandidateIndex] || !Candidates[CandidateIndex].bValid)
					{
						continue;
					}

					if (!RectsOverlap(Candidates[CurrentIndex].Rect, Candidates[CandidateIndex].Rect))
					{
						continue;
					}

					GroupIndices.Add(CandidateIndex);
					bVisited[CandidateIndex] = 1;
				}
			}

			if (GroupIndices.Num() <= 1 || GroupIndices.Num() > MaxCompositeSmokeSlots)
			{
				continue;
			}

			TArray<FRenderSmokeState*> GroupRenderStates;
			GroupRenderStates.Reserve(GroupIndices.Num());
			for (const int32 GroupIndex : GroupIndices)
			{
				GroupRenderStates.Add(Candidates[GroupIndex].State);
			}

			const FScreenPassTexture MultiOutput = CompositeSmokeMulti_RenderThread(
				GraphBuilder,
				View,
				Inputs,
				GroupRenderStates,
				CurrentSceneColor,
				InvViewProjection,
				false);
			if (MultiOutput.Texture == CurrentSceneColor.Texture)
			{
				continue;
			}

			CurrentSceneColor = MultiOutput;
			for (const int32 GroupIndex : GroupIndices)
			{
				bHandledByMulti[GroupIndex] = 1;
			}
		}
	}

	struct FLegacyCompositeEntry
	{
		int32 StateIndex = INDEX_NONE;
		FIntRect SmokeRect;
	};

	TArray<FLegacyCompositeEntry> LegacyEntries;
	LegacyEntries.Reserve(RenderStates.Num());
	for (int32 StateIndex = 0; StateIndex < RenderStates.Num(); ++StateIndex)
	{
		if (bHandledByMulti[StateIndex])
		{
			continue;
		}

		FLegacyCompositeEntry Entry;
		Entry.StateIndex = StateIndex;
		if (!Candidates[StateIndex].bValid)
		{
			continue;
		}
		Entry.SmokeRect = bUseScissor ? Candidates[StateIndex].Rect : CurrentSceneColor.ViewRect;
		LegacyEntries.Add(Entry);
	}
	if (LegacyEntries.IsEmpty())
	{
		return ReturnCurrentOrOverrideOutput();
	}

	for (int32 EntryIndex = 0; EntryIndex < LegacyEntries.Num(); ++EntryIndex)
	{
		const int32 StateIndex = LegacyEntries[EntryIndex].StateIndex;
		FRenderSmokeState& State = *RenderStates[StateIndex];
		const bool bIsLastSmoke = EntryIndex == LegacyEntries.Num() - 1;
		const FScreenPassTextureViewport InputViewport(CurrentSceneColor);
		const bool bUseOverrideOutput = bIsLastSmoke &&
			Inputs.OverrideOutput.IsValid() &&
			Inputs.OverrideOutput.Texture != CurrentSceneColor.Texture;

		FScreenPassRenderTarget Output = bUseOverrideOutput
			? Inputs.OverrideOutput
			: FScreenPassRenderTarget::CreateFromInput(GraphBuilder, CurrentSceneColor, ERenderTargetLoadAction::ELoad, TEXT("TimeThiefSmoke.Composite"));

		FIntRect SmokeRect = bUseScissor ? LegacyEntries[EntryIndex].SmokeRect : Output.ViewRect;
		SmokeRect.Min.X = FMath::Clamp(SmokeRect.Min.X, Output.ViewRect.Min.X, Output.ViewRect.Max.X);
		SmokeRect.Min.Y = FMath::Clamp(SmokeRect.Min.Y, Output.ViewRect.Min.Y, Output.ViewRect.Max.Y);
		SmokeRect.Max.X = FMath::Clamp(SmokeRect.Max.X, Output.ViewRect.Min.X, Output.ViewRect.Max.X);
		SmokeRect.Max.Y = FMath::Clamp(SmokeRect.Max.Y, Output.ViewRect.Min.Y, Output.ViewRect.Max.Y);
		if (SmokeRect.Width() <= 0 || SmokeRect.Height() <= 0)
		{
			continue;
		}

		const bool bUseFullscreenComposite = ShouldUseFullscreenComposite(SmokeRect, Output.ViewRect);
		if (bUseFullscreenComposite || Output.Texture != CurrentSceneColor.Texture)
		{
			SmokeRect = Output.ViewRect;
		}

		TArray<FTimeThiefSmokeEventShaderData> ShaderEvents;
		ShaderEvents.Reserve(State.LastDebugEvents.Num() + State.AnalyticBulletEvents.Num());
		int32 AnalyticBulletEventCount = 0;
		for (const FTimeThiefSmokeRendererEvent& Event : State.AnalyticBulletEvents)
		{
			if (ShaderEvents.Num() >= MaxDebugEventCount)
			{
				break;
			}
			ShaderEvents.Add(ToShaderEvent(Event));
			++AnalyticBulletEventCount;
		}
		for (const FTimeThiefSmokeRendererEvent& Event : State.LastDebugEvents)
		{
			if (ShaderEvents.Num() >= MaxDebugEventCount)
			{
				break;
			}
			if (Event.Type == ETimeThiefSmokeRendererInteractionType::BulletWake)
			{
				continue;
			}
			ShaderEvents.Add(ToShaderEvent(Event));
		}
		const int32 ShaderEventCount = ShaderEvents.Num();
		if (ShaderEvents.IsEmpty())
		{
			ShaderEvents.AddDefaulted();
		}

		FRDGBufferRef EventBuffer = GraphBuilder.CreateBuffer(
			FRDGBufferDesc::CreateStructuredDesc(sizeof(FTimeThiefSmokeEventShaderData), ShaderEvents.Num()),
			TEXT("TimeThiefSmoke.CompositeEvents"));
		GraphBuilder.QueueBufferUpload(EventBuffer, ShaderEvents.GetData(), ShaderEvents.Num() * sizeof(FTimeThiefSmokeEventShaderData));

		TShaderMapRef<FTimeThiefSmokeCompositePS> PixelShader(GetGlobalShaderMap(View.FeatureLevel));
		FTimeThiefSmokeCompositePS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeCompositePS::FParameters>();
		PassParameters->SceneColorTexture = CurrentSceneColor.Texture;
		PassParameters->SceneDepthTexture = Inputs.SceneTextures.SceneTextures->GetParameters()->SceneDepthTexture;
		PassParameters->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		PassParameters->DensityTexture = GraphBuilder.RegisterExternalTexture(State.DensityTextures[State.CurrentDensityIndex]);
		PassParameters->DivergenceTexture = GraphBuilder.RegisterExternalTexture(State.DivergenceTexture);
		PassParameters->BrickOccupancyTexture = GraphBuilder.RegisterExternalTexture(State.BrickOccupancyTexture);
		PassParameters->SparseFieldAtlasTexture = GraphBuilder.RegisterExternalTexture(State.SparseFieldAtlasTexture);
		PassParameters->WarpTexture = GraphBuilder.RegisterExternalTexture(State.WarpTextures[State.CurrentWarpIndex]);
		PassParameters->ObstacleTexture = GraphBuilder.RegisterExternalTexture(State.ObstacleTexture);
		PassParameters->BulletCutoutTexture = GraphBuilder.RegisterExternalTexture(State.BulletCutoutTextures[State.CurrentBulletFieldIndex]);
		PassParameters->BulletSinkTexture = GraphBuilder.RegisterExternalTexture(State.BulletSinkTextures[State.CurrentBulletFieldIndex]);
		PassParameters->VolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		PassParameters->Events = GraphBuilder.CreateSRV(EventBuffer);
		const FVector2f SceneColorTextureExtent(
			static_cast<float>(FMath::Max(1, CurrentSceneColor.Texture->Desc.Extent.X)),
			static_cast<float>(FMath::Max(1, CurrentSceneColor.Texture->Desc.Extent.Y)));
		const FVector2f InputRectMin(
			static_cast<float>(CurrentSceneColor.ViewRect.Min.X),
			static_cast<float>(CurrentSceneColor.ViewRect.Min.Y));
		const FVector2f OutputRectMin(
			static_cast<float>(Output.ViewRect.Min.X),
			static_cast<float>(Output.ViewRect.Min.Y));
		const FVector2f InputToOutputScale(
			static_cast<float>(CurrentSceneColor.ViewRect.Width()) / static_cast<float>(FMath::Max(1, Output.ViewRect.Width())),
			static_cast<float>(CurrentSceneColor.ViewRect.Height()) / static_cast<float>(FMath::Max(1, Output.ViewRect.Height())));
		PassParameters->SceneColorUVScaleBias = FVector4f(
			InputToOutputScale.X / SceneColorTextureExtent.X,
			InputToOutputScale.Y / SceneColorTextureExtent.Y,
			(InputRectMin.X - OutputRectMin.X * InputToOutputScale.X) / SceneColorTextureExtent.X,
			(InputRectMin.Y - OutputRectMin.Y * InputToOutputScale.Y) / SceneColorTextureExtent.Y);
		PassParameters->ViewRect = Output.ViewRect;
		PassParameters->GridResolution = State.AllocatedGridSize;
		const bool bUseSparseComposite = State.Volume.Settings.SimulationBackend == ETimeThiefSmokeSimulationBackend::SparseMac &&
			!State.bForceDenseComposite &&
			ShouldUseSparseComposite(State.AllocatedBrickGridSize, State.LastActiveBrickCount);
		PassParameters->bUseSparseAtlas = bUseSparseComposite ? 1u : 0u;
		PassParameters->MaxActiveSmokeBricks = FMath::Max(State.Volume.Settings.MaxActiveSmokeBricks, 1);
		PassParameters->BrickGridResolution = State.AllocatedBrickGridSize;
		PassParameters->SparseAtlasBrickGridResolution = State.AllocatedSparseAtlasBrickGridSize;
		PassParameters->SmokeBrickSize = FMath::Clamp(State.Volume.Settings.SmokeBrickSize, 4, 32);
		PassParameters->BoundsExtent = FVector3f(State.Volume.BoundsExtent);
		PassParameters->NaturalBoundsExtent = FVector3f(State.Volume.NaturalBoundsExtent);
		PassParameters->RenderBoundsExtent = FVector3f(State.Volume.RenderBoundsExtent);
		PassParameters->Extinction = State.Volume.Settings.Extinction;
		PassParameters->ScatteringAlbedo = State.Volume.Settings.ScatteringAlbedo;
		PassParameters->ScatteringAnisotropy = State.Volume.Settings.ScatteringAnisotropy;
		PassParameters->RenderNoiseScale = State.Volume.Settings.RenderNoiseScale;
		PassParameters->RenderNoiseStrength = State.Volume.Settings.RenderNoiseStrength;
		PassParameters->RenderNoiseTimeScale = State.Volume.Settings.RenderNoiseTimeScale;
		PassParameters->RenderFilamentScale = State.Volume.Settings.RenderFilamentScale;
		PassParameters->RenderFilamentStrength = State.Volume.Settings.RenderFilamentStrength;
		PassParameters->RenderFilamentContrast = State.Volume.Settings.RenderFilamentContrast;
		PassParameters->RenderFilamentWarpStrength = State.Volume.Settings.RenderFilamentWarpStrength;
		PassParameters->AgeSeconds = State.Volume.AgeSeconds;
		PassParameters->DurationSeconds = State.Volume.DurationSeconds;
		PassParameters->SmokeFadeOutDuration = State.Volume.Settings.SmokeFadeOutDuration;
		PassParameters->RenderStepCount = FMath::Clamp(State.Volume.Settings.RenderStepCount, 8, 512);
		PassParameters->RenderMaxStepCount = FMath::Clamp(State.Volume.Settings.RenderMaxStepCount, 16, 1024);
		PassParameters->RenderStepVoxelScale = FMath::Clamp(State.Volume.Settings.RenderStepVoxelScale, 0.1f, 4.0f);
		PassParameters->bUseFastFilament = CVarTimeThiefSmokeFastFilament.GetValueOnRenderThread() != 0 ? 1u : 0u;
		PassParameters->DebugMode = DebugMode;
		PassParameters->EventCount = ShaderEventCount;
		PassParameters->AnalyticBulletEventCount = AnalyticBulletEventCount;
		PassParameters->InvViewProjection = InvViewProjection;
		PassParameters->LocalToWorld = State.Volume.LocalToWorld.ToMatrixWithScale();
		PassParameters->WorldToLocal = State.Volume.LocalToWorld.ToInverseMatrixWithScale();
		PassParameters->RenderTargets[0] = Output.GetRenderTargetBinding();

		AddDrawScreenPass(
			GraphBuilder,
			RDG_EVENT_NAME("TimeThiefSmoke.Composite SmokeId=%d", State.Volume.SmokeId),
			ViewInfo,
			FScreenPassTextureViewport(Output.Texture, SmokeRect),
			InputViewport,
			PixelShader,
			PassParameters);

		CurrentSceneColor = MoveTemp(Output);
	}

	return ReturnCurrentOrOverrideOutput();
}

void FTimeThiefSmokeViewExtension::EnsureResources(FRDGBuilder& GraphBuilder, FRenderSmokeState& State)
{
	const FIntVector GridSize = State.AllocatedGridSize;
	const FRDGTextureDesc DensityDesc = FRDGTextureDesc::Create3D(
		GridSize,
		PF_R16F,
		FClearValueBinding::Black,
		TexCreate_ShaderResource | TexCreate_UAV);
	const FRDGTextureDesc VelocityDesc = FRDGTextureDesc::Create3D(
		GridSize,
		PF_FloatRGBA,
		FClearValueBinding::Black,
		TexCreate_ShaderResource | TexCreate_UAV);
	const FRDGTextureDesc ScalarDesc = FRDGTextureDesc::Create3D(
		GridSize,
		PF_R16F,
		FClearValueBinding::Black,
		TexCreate_ShaderResource | TexCreate_UAV);
	const bool bSparseBackend = State.Volume.Settings.SimulationBackend == ETimeThiefSmokeSimulationBackend::SparseMac;
	const int32 BrickSize = FMath::Clamp(State.Volume.Settings.SmokeBrickSize, 4, 32);
	const FIntVector BrickGridSize = bSparseBackend ? MakeBrickGridSize(GridSize, BrickSize) : FIntVector(1, 1, 1);
	const FIntVector SparseAtlasBrickGridSize = bSparseBackend ? MakeSparseAtlasBrickGridSize(State.Volume.Settings.MaxActiveSmokeBricks) : FIntVector(1, 1, 1);
	const FIntVector SparseAtlasGridSize = bSparseBackend ? MakeSparseAtlasGridSize(SparseAtlasBrickGridSize, BrickSize) : FIntVector(1, 1, 1);
	const FRDGTextureDesc CurlDesc = FRDGTextureDesc::Create3D(
		GridSize,
		PF_FloatRGBA,
		FClearValueBinding::Black,
		TexCreate_ShaderResource | TexCreate_UAV);
	const FRDGTextureDesc BrickOccupancyDesc = FRDGTextureDesc::Create3D(
		BrickGridSize,
		PF_R32_UINT,
		FClearValueBinding::Black,
		TexCreate_ShaderResource | TexCreate_UAV);
	const FRDGTextureDesc SparseAtlasDesc = FRDGTextureDesc::Create3D(
		SparseAtlasGridSize,
		PF_FloatRGBA,
		FClearValueBinding::Black,
		TexCreate_ShaderResource | TexCreate_UAV);

	for (int32 TextureIndex = 0; TextureIndex < 2; ++TextureIndex)
	{
		if (!State.DensityTextures[TextureIndex].IsValid())
		{
			AllocatePooledTexture(DensityDesc, State.DensityTextures[TextureIndex], TEXT("TimeThiefSmoke.Density"));
			State.bNeedsInit = true;
		}

		if (!State.VelocityTextures[TextureIndex].IsValid())
		{
			AllocatePooledTexture(VelocityDesc, State.VelocityTextures[TextureIndex], TEXT("TimeThiefSmoke.Velocity"));
			State.bNeedsInit = true;
		}

		if (bSparseBackend && !State.MacVelocityUTextures[TextureIndex].IsValid())
		{
			AllocatePooledTexture(ScalarDesc, State.MacVelocityUTextures[TextureIndex], TEXT("TimeThiefSmoke.MacVelocityU"));
		}
		else if (!bSparseBackend)
		{
			State.MacVelocityUTextures[TextureIndex].SafeRelease();
		}

		if (bSparseBackend && !State.MacVelocityVTextures[TextureIndex].IsValid())
		{
			AllocatePooledTexture(ScalarDesc, State.MacVelocityVTextures[TextureIndex], TEXT("TimeThiefSmoke.MacVelocityV"));
		}
		else if (!bSparseBackend)
		{
			State.MacVelocityVTextures[TextureIndex].SafeRelease();
		}

		if (bSparseBackend && !State.MacVelocityWTextures[TextureIndex].IsValid())
		{
			AllocatePooledTexture(ScalarDesc, State.MacVelocityWTextures[TextureIndex], TEXT("TimeThiefSmoke.MacVelocityW"));
		}
		else if (!bSparseBackend)
		{
			State.MacVelocityWTextures[TextureIndex].SafeRelease();
		}

		if (!State.PressureTextures[TextureIndex].IsValid())
		{
			AllocatePooledTexture(ScalarDesc, State.PressureTextures[TextureIndex], TEXT("TimeThiefSmoke.Pressure"));
			State.bNeedsInit = true;
		}

		if (!State.BulletCutoutTextures[TextureIndex].IsValid())
		{
			AllocatePooledTexture(ScalarDesc, State.BulletCutoutTextures[TextureIndex], TEXT("TimeThiefSmoke.BulletCutout"));
			State.bNeedsInit = true;
		}

		if (!State.BulletSinkTextures[TextureIndex].IsValid())
		{
			AllocatePooledTexture(ScalarDesc, State.BulletSinkTextures[TextureIndex], TEXT("TimeThiefSmoke.BulletSink"));
			State.bNeedsInit = true;
		}

		if (!State.WarpTextures[TextureIndex].IsValid())
		{
			AllocatePooledTexture(ScalarDesc, State.WarpTextures[TextureIndex], TEXT("TimeThiefSmoke.Warp"));
			State.bNeedsInit = true;
		}
	}

	if (!State.DivergenceTexture.IsValid())
	{
		AllocatePooledTexture(ScalarDesc, State.DivergenceTexture, TEXT("TimeThiefSmoke.Divergence"));
		State.bNeedsInit = true;
	}

	if (!State.CurlTexture.IsValid())
	{
		AllocatePooledTexture(CurlDesc, State.CurlTexture, TEXT("TimeThiefSmoke.Curl"));
	}

	if (!State.BrickOccupancyTexture.IsValid() || State.AllocatedBrickGridSize != BrickGridSize)
	{
		State.BrickOccupancyTexture.SafeRelease();
		AllocatePooledTexture(BrickOccupancyDesc, State.BrickOccupancyTexture, TEXT("TimeThiefSmoke.BrickOccupancy"));
		State.AllocatedBrickGridSize = BrickGridSize;
	}

	if (!State.SparseFieldAtlasTexture.IsValid() ||
		State.AllocatedSparseAtlasGridSize != SparseAtlasGridSize)
	{
		State.SparseFieldAtlasTexture.SafeRelease();
		AllocatePooledTexture(SparseAtlasDesc, State.SparseFieldAtlasTexture, TEXT("TimeThiefSmoke.SparseFieldAtlas"));
		State.AllocatedSparseAtlasBrickGridSize = SparseAtlasBrickGridSize;
		State.AllocatedSparseAtlasGridSize = SparseAtlasGridSize;
	}

	EnsureObstacleTexture(GraphBuilder, State);
}

void FTimeThiefSmokeViewExtension::EnsureObstacleTexture(FRDGBuilder& GraphBuilder, FRenderSmokeState& State)
{
	const int32 SourceResolution = State.Volume.ObstacleMaskResolution;
	const int32 SourceVoxelCount = SourceResolution > 0 ? SourceResolution * SourceResolution * SourceResolution : 0;
	const bool bHasValidMask = SourceResolution > 0 && State.Volume.ObstacleMask.Num() == SourceVoxelCount;
	const int32 DesiredResolution = bHasValidMask ? FMath::Clamp(SourceResolution, 1, 128) : 1;

	if (!State.ObstacleTexture.IsValid() || State.AllocatedObstacleResolution != DesiredResolution)
	{
		State.ObstacleTexture.SafeRelease();
		const FRDGTextureDesc ObstacleDesc = FRDGTextureDesc::Create3D(
			FIntVector(DesiredResolution, DesiredResolution, DesiredResolution),
			PF_G8,
			FClearValueBinding::Black,
			TexCreate_ShaderResource);
		AllocatePooledTexture(ObstacleDesc, State.ObstacleTexture, TEXT("TimeThiefSmoke.ObstacleMask"));
		State.AllocatedObstacleResolution = DesiredResolution;
		State.UploadedObstacleMaskRevision = MAX_uint32;
		State.TargetObstacleMaskRevision = MAX_uint32;
		State.ObstacleMaskBlendAge = 1.0f;
		State.ObstacleUploadScratch.Reset();
		State.ObstaclePreviousScratch.Reset();
		State.ObstacleTargetScratch.Reset();
	}

	if (!State.ObstacleTexture.IsValid())
	{
		return;
	}

	const int32 DesiredVoxelCount = DesiredResolution * DesiredResolution * DesiredResolution;
	if (State.TargetObstacleMaskRevision != State.Volume.ObstacleMaskRevision)
	{
		const bool bHasUploadedMask = State.UploadedObstacleMaskRevision != MAX_uint32 &&
			State.ObstacleUploadScratch.Num() == DesiredVoxelCount;
		State.ObstaclePreviousScratch.SetNumZeroed(DesiredVoxelCount);
		if (bHasUploadedMask)
		{
			FMemory::Memcpy(
				State.ObstaclePreviousScratch.GetData(),
				State.ObstacleUploadScratch.GetData(),
				DesiredVoxelCount * sizeof(uint8));
		}

		State.ObstacleTargetScratch.SetNumZeroed(DesiredVoxelCount);
		if (bHasValidMask)
		{
			FMemory::Memcpy(
				State.ObstacleTargetScratch.GetData(),
				State.Volume.ObstacleMask.GetData(),
				DesiredVoxelCount * sizeof(uint8));
		}

		State.TargetObstacleMaskRevision = State.Volume.ObstacleMaskRevision;
		State.ObstacleMaskBlendAge = bHasUploadedMask ? 0.0f : ObstacleMaskBlendDuration;
	}

	if (State.UploadedObstacleMaskRevision == State.TargetObstacleMaskRevision && State.ObstacleMaskBlendAge >= ObstacleMaskBlendDuration)
	{
		return;
	}

	State.ObstacleUploadScratch.SetNumZeroed(DesiredVoxelCount);
	const float BlendAlpha = FMath::Clamp(State.ObstacleMaskBlendAge / ObstacleMaskBlendDuration, 0.0f, 1.0f);
	for (int32 VoxelIndex = 0; VoxelIndex < DesiredVoxelCount; ++VoxelIndex)
	{
		const float PreviousValue = State.ObstaclePreviousScratch.IsValidIndex(VoxelIndex) ? State.ObstaclePreviousScratch[VoxelIndex] : 0.0f;
		const float TargetValue = State.ObstacleTargetScratch.IsValidIndex(VoxelIndex) ? State.ObstacleTargetScratch[VoxelIndex] : 0.0f;
		State.ObstacleUploadScratch[VoxelIndex] = static_cast<uint8>(FMath::RoundToInt(FMath::Lerp(PreviousValue, TargetValue, BlendAlpha)));
	}

	const FUpdateTextureRegion3D UpdateRegion(0, 0, 0, 0, 0, 0, DesiredResolution, DesiredResolution, DesiredResolution);
	GraphBuilder.RHICmdList.UpdateTexture3D(
		State.ObstacleTexture->GetRHI(),
		0,
		UpdateRegion,
		DesiredResolution * sizeof(uint8),
		DesiredResolution * DesiredResolution * sizeof(uint8),
		State.ObstacleUploadScratch.GetData());

	State.ObstacleMaskBlendAge += FMath::Max(LastFrameDeltaSeconds, 0.0f);
	if (State.ObstacleMaskBlendAge >= ObstacleMaskBlendDuration)
	{
		State.UploadedObstacleMaskRevision = State.TargetObstacleMaskRevision;
		if (State.ObstacleTargetScratch.Num() == DesiredVoxelCount)
		{
			FMemory::Memcpy(
				State.ObstacleUploadScratch.GetData(),
				State.ObstacleTargetScratch.GetData(),
				DesiredVoxelCount * sizeof(uint8));
		}
	}
}

void FTimeThiefSmokeViewExtension::EnsureVortexParticleBuffers(FRDGBuilder& GraphBuilder, FRenderSmokeState& State)
{
	const int32 DesiredCount = FMath::Clamp(State.Volume.Settings.VortexParticleCount, 1, MaxVortexParticleCount);
	if (State.AllocatedVortexParticleCount != DesiredCount)
	{
		State.AllocatedVortexParticleCount = DesiredCount;
		State.VortexParticleBuffers[0].SafeRelease();
		State.VortexParticleBuffers[1].SafeRelease();
		State.CurrentVortexParticleIndex = 0;
		State.bVortexParticlesNeedUpload = true;
	}

	FRDGBufferDesc VortexBufferDesc = FRDGBufferDesc::CreateStructuredDesc(
		sizeof(FTimeThiefSmokeVortexParticleShaderData),
		DesiredCount);
	VortexBufferDesc.Usage |=
		EBufferUsageFlags::ShaderResource |
		EBufferUsageFlags::UnorderedAccess |
		EBufferUsageFlags::SourceCopy;

	for (int32 BufferIndex = 0; BufferIndex < 2; ++BufferIndex)
	{
		if (!State.VortexParticleBuffers[BufferIndex].IsValid())
		{
			AllocatePooledBuffer(VortexBufferDesc, State.VortexParticleBuffers[BufferIndex], TEXT("TimeThiefSmoke.VortexParticles"));
			State.bVortexParticlesNeedUpload = true;
		}
	}
}

void FTimeThiefSmokeViewExtension::UploadDeadVortexParticles(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGBufferRef VortexBuffer)
{
	const int32 ParticleCount = FMath::Clamp(State.AllocatedVortexParticleCount, 1, MaxVortexParticleCount);
	TArray<FTimeThiefSmokeVortexParticleShaderData> ShaderParticles;
	ShaderParticles.AddDefaulted(ParticleCount);

	FRDGBufferRef UploadBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(FTimeThiefSmokeVortexParticleShaderData), ShaderParticles.Num()),
		TEXT("TimeThiefSmoke.VortexUpload"));
	GraphBuilder.QueueBufferUpload(UploadBuffer, ShaderParticles.GetData(), ShaderParticles.Num() * sizeof(FTimeThiefSmokeVortexParticleShaderData));
	AddCopyBufferPass(GraphBuilder, VortexBuffer, UploadBuffer);
}

void FTimeThiefSmokeViewExtension::AddVortexParticleUpdatePass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGBufferRef VortexIn,
	FRDGBufferRef VortexOut,
	FRDGTextureRef DensityIn,
	FRDGTextureRef VelocityIn,
	FRDGTextureRef BulletCutoutTexture,
	FRDGTextureRef BulletSinkTexture,
	FRDGBufferRef EventBuffer,
	int32 EventCount,
	float DeltaSeconds)
{
	const int32 VortexParticleCount = FMath::Clamp(State.Volume.Settings.VortexParticleCount, 1, MaxVortexParticleCount);
	TShaderMapRef<FTimeThiefSmokeUpdateVortexParticlesCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokeUpdateVortexParticlesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeUpdateVortexParticlesCS::FParameters>();
	PassParameters->VortexParticlesIn = GraphBuilder.CreateSRV(VortexIn);
	PassParameters->OutVortexParticles = GraphBuilder.CreateUAV(VortexOut);
	PassParameters->DensityIn = DensityIn;
	PassParameters->VelocityIn = VelocityIn;
	PassParameters->ObstacleTexture = GraphBuilder.RegisterExternalTexture(State.ObstacleTexture);
	PassParameters->BulletCutoutTexture = BulletCutoutTexture;
	PassParameters->BulletSinkTexture = BulletSinkTexture;
	PassParameters->VolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->Events = GraphBuilder.CreateSRV(EventBuffer);
	PassParameters->GridResolution = State.AllocatedGridSize;
	PassParameters->BoundsExtent = FVector3f(State.Volume.BoundsExtent);
	PassParameters->DeltaSeconds = DeltaSeconds;
	PassParameters->AgeSeconds = State.Volume.AgeSeconds;
	PassParameters->VortexParticleCount = VortexParticleCount;
	PassParameters->VortexParticleLifeSeconds = FMath::Max(0.05f, State.Volume.Settings.VortexParticleLifeSeconds);
	PassParameters->VortexParticleStrength = State.Volume.Settings.VortexParticleStrength;
	PassParameters->VortexDensityGradientScale = FMath::Max(0.0f, State.Volume.Settings.VortexDensityGradientScale);
	PassParameters->SelfWobbleTimeScale = State.Volume.Settings.SelfWobbleTimeScale;
	PassParameters->SelfWobbleParticleScale = State.Volume.Settings.SelfWobbleParticleScale;
	PassParameters->EventCount = EventCount;
	PassParameters->LocalToWorld = State.Volume.LocalToWorld.ToMatrixWithScale();
	PassParameters->WorldToLocal = State.Volume.LocalToWorld.ToInverseMatrixWithScale();

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.UpdateVortexParticles SmokeId=%d", State.Volume.SmokeId),
		ComputeShader,
		PassParameters,
		FIntVector(FMath::DivideAndRoundUp(VortexParticleCount, 64), 1, 1));
}

FRDGBufferRef FTimeThiefSmokeViewExtension::AddBuildVortexBrickMasksPass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGBufferRef VortexBuffer)
{
	const FIntVector BrickGridSize = State.AllocatedBrickGridSize;
	const uint32 BrickCount = static_cast<uint32>(FMath::Max(1, BrickGridSize.X * BrickGridSize.Y * BrickGridSize.Z));
	FRDGBufferRef VortexBrickMasksBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32) * 4, BrickCount),
		TEXT("TimeThiefSmoke.VortexBrickMasks"));

	TShaderMapRef<FTimeThiefSmokeBuildVortexBrickMasksCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokeBuildVortexBrickMasksCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeBuildVortexBrickMasksCS::FParameters>();
	PassParameters->VortexParticles = GraphBuilder.CreateSRV(VortexBuffer);
	PassParameters->OutVortexBrickMasks = GraphBuilder.CreateUAV(VortexBrickMasksBuffer);
	PassParameters->VortexParticleCount = FMath::Clamp(State.Volume.Settings.VortexParticleCount, 1, MaxVortexParticleCount);
	PassParameters->VortexParticleSplatRadius = FMath::Max(1.0f, State.Volume.Settings.VortexParticleSplatRadius);
	PassParameters->BrickGridResolution = BrickGridSize;
	PassParameters->SmokeBrickSize = FMath::Clamp(State.Volume.Settings.SmokeBrickSize, 4, 32);
	PassParameters->BoundsExtent = FVector3f(State.Volume.BoundsExtent);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.BuildVortexBrickMasks SmokeId=%d", State.Volume.SmokeId),
		ComputeShader,
		PassParameters,
		MakeGroupCount(BrickGridSize));

	return VortexBrickMasksBuffer;
}

void FTimeThiefSmokeViewExtension::AddVortexParticleSplatPass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGBufferRef VortexBuffer,
	FRDGBufferRef VortexBrickMasksBuffer,
	FRDGTextureRef DensityIn,
	FRDGTextureRef VelocityIn,
	FRDGTextureRef BulletCutoutTexture,
	FRDGTextureRef BulletSinkTexture,
	FRDGTextureRef VelocityOut)
{
	const FIntVector GridSize = State.AllocatedGridSize;
	const FIntVector GroupCount = MakeGroupCount(GridSize);

	TShaderMapRef<FTimeThiefSmokeSplatVortexParticlesCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokeSplatVortexParticlesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeSplatVortexParticlesCS::FParameters>();
	PassParameters->VortexParticles = GraphBuilder.CreateSRV(VortexBuffer);
	PassParameters->VortexBrickMasks = GraphBuilder.CreateSRV(VortexBrickMasksBuffer);
	PassParameters->DensityIn = DensityIn;
	PassParameters->VelocityIn = VelocityIn;
	PassParameters->ObstacleTexture = GraphBuilder.RegisterExternalTexture(State.ObstacleTexture);
	PassParameters->BrickOccupancyTexture = GraphBuilder.RegisterExternalTexture(State.BrickOccupancyTexture);
	PassParameters->BulletCutoutTexture = BulletCutoutTexture;
	PassParameters->BulletSinkTexture = BulletSinkTexture;
	PassParameters->VolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->OutVelocity = GraphBuilder.CreateUAV(VelocityOut);
	PassParameters->GridResolution = GridSize;
	PassParameters->BoundsExtent = FVector3f(State.Volume.BoundsExtent);
	PassParameters->VortexParticleCount = FMath::Clamp(State.Volume.Settings.VortexParticleCount, 1, MaxVortexParticleCount);
	PassParameters->VortexParticleSplatRadius = FMath::Max(1.0f, State.Volume.Settings.VortexParticleSplatRadius);
	PassParameters->VortexParticleCoreRadius = FMath::Max(1.0f, State.Volume.Settings.VortexParticleCoreRadius);
	PassParameters->BrickGridResolution = State.AllocatedBrickGridSize;
	PassParameters->SmokeBrickSize = FMath::Clamp(State.Volume.Settings.SmokeBrickSize, 4, 32);
	PassParameters->bUseVortexBrickBins = State.Volume.Settings.bUseVortexBrickBins ? 1u : 0u;
	PassParameters->bUseSparseSimulationMask = State.bUseSparseSimulationMaskThisFrame ? 1u : 0u;

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.SplatVortexParticles SmokeId=%d", State.Volume.SmokeId),
		ComputeShader,
		PassParameters,
		GroupCount);
}

FRDGBufferRef FTimeThiefSmokeViewExtension::AddBuildEventBrickMasksPass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGBufferRef EventBuffer,
	int32 EventCount,
	const TCHAR* DebugName)
{
	if (EventCount <= 0)
	{
		FRDGBufferRef EventBrickMasksBuffer = GraphBuilder.CreateBuffer(
			FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32) * 4, 1),
			DebugName);
		TArray<uint32> EmptyMask;
		EmptyMask.AddZeroed(4);
		GraphBuilder.QueueBufferUpload(EventBrickMasksBuffer, EmptyMask.GetData(), EmptyMask.Num() * sizeof(uint32));
		return EventBrickMasksBuffer;
	}

	const int32 BrickMaskCount = FMath::Max(1, State.AllocatedBrickGridSize.X * State.AllocatedBrickGridSize.Y * State.AllocatedBrickGridSize.Z);
	FRDGBufferRef EventBrickMasksBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32) * 4, BrickMaskCount),
		DebugName);

	TShaderMapRef<FTimeThiefSmokeBuildEventBrickMasksCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokeBuildEventBrickMasksCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeBuildEventBrickMasksCS::FParameters>();
	PassParameters->GridResolution = State.AllocatedGridSize;
	PassParameters->BrickGridResolution = State.AllocatedBrickGridSize;
	PassParameters->SmokeBrickSize = FMath::Clamp(State.Volume.Settings.SmokeBrickSize, 4, 32);
	PassParameters->BoundsExtent = FVector3f(State.Volume.BoundsExtent);
	PassParameters->EventCount = EventCount;
	PassParameters->LocalToWorld = State.Volume.LocalToWorld.ToMatrixWithScale();
	PassParameters->Events = GraphBuilder.CreateSRV(EventBuffer);
	PassParameters->ActorAirflowRadiusScale = FMath::Max(0.1f, State.Volume.Settings.ActorAirflowRadiusScale);
	PassParameters->OutEventBrickMasks = GraphBuilder.CreateUAV(EventBrickMasksBuffer);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.BuildEventBrickMasks SmokeId=%d Events=%d", State.Volume.SmokeId, EventCount),
		ComputeShader,
		PassParameters,
		State.AllocatedBrickGridSize);

	return EventBrickMasksBuffer;
}

void FTimeThiefSmokeViewExtension::SimulateSmoke(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	float DeltaSeconds)
{
	EnsureResources(GraphBuilder, State);
	EnsureVortexParticleBuffers(GraphBuilder, State);
	const bool bUseMacProjection = State.Volume.Settings.SimulationBackend == ETimeThiefSmokeSimulationBackend::SparseMac;

	FRDGTextureRef DensityTextures[2] =
	{
		GraphBuilder.RegisterExternalTexture(State.DensityTextures[0]),
		GraphBuilder.RegisterExternalTexture(State.DensityTextures[1])
	};
	FRDGTextureRef VelocityTextures[2] =
	{
		GraphBuilder.RegisterExternalTexture(State.VelocityTextures[0]),
		GraphBuilder.RegisterExternalTexture(State.VelocityTextures[1])
	};
	FRDGTextureRef MacVelocityUTextures[2] = {};
	FRDGTextureRef MacVelocityVTextures[2] = {};
	FRDGTextureRef MacVelocityWTextures[2] = {};
	if (bUseMacProjection)
	{
		MacVelocityUTextures[0] = GraphBuilder.RegisterExternalTexture(State.MacVelocityUTextures[0]);
		MacVelocityUTextures[1] = GraphBuilder.RegisterExternalTexture(State.MacVelocityUTextures[1]);
		MacVelocityVTextures[0] = GraphBuilder.RegisterExternalTexture(State.MacVelocityVTextures[0]);
		MacVelocityVTextures[1] = GraphBuilder.RegisterExternalTexture(State.MacVelocityVTextures[1]);
		MacVelocityWTextures[0] = GraphBuilder.RegisterExternalTexture(State.MacVelocityWTextures[0]);
		MacVelocityWTextures[1] = GraphBuilder.RegisterExternalTexture(State.MacVelocityWTextures[1]);
	}
	FRDGTextureRef PressureTextures[2] =
	{
		GraphBuilder.RegisterExternalTexture(State.PressureTextures[0]),
		GraphBuilder.RegisterExternalTexture(State.PressureTextures[1])
	};
	FRDGTextureRef BulletCutoutTextures[2] =
	{
		GraphBuilder.RegisterExternalTexture(State.BulletCutoutTextures[0]),
		GraphBuilder.RegisterExternalTexture(State.BulletCutoutTextures[1])
	};
	FRDGTextureRef BulletSinkTextures[2] =
	{
		GraphBuilder.RegisterExternalTexture(State.BulletSinkTextures[0]),
		GraphBuilder.RegisterExternalTexture(State.BulletSinkTextures[1])
	};
	FRDGTextureRef WarpTextures[2] =
	{
		GraphBuilder.RegisterExternalTexture(State.WarpTextures[0]),
		GraphBuilder.RegisterExternalTexture(State.WarpTextures[1])
	};
	FRDGTextureRef CurlTexture = GraphBuilder.RegisterExternalTexture(State.CurlTexture);
	FRDGTextureRef DivergenceTexture = GraphBuilder.RegisterExternalTexture(State.DivergenceTexture);
	FRDGTextureRef ObstacleTexture = GraphBuilder.RegisterExternalTexture(State.ObstacleTexture);
	FRDGBufferRef VortexParticleBuffers[2] =
	{
		GraphBuilder.RegisterExternalBuffer(State.VortexParticleBuffers[0], TEXT("TimeThiefSmoke.VortexParticles0")),
		GraphBuilder.RegisterExternalBuffer(State.VortexParticleBuffers[1], TEXT("TimeThiefSmoke.VortexParticles1"))
	};
	int32 BulletEventCount = 0;
	TArray<FTimeThiefSmokeRendererEvent> AdvectionEvents = State.PendingEvents;
	for (const FTimeThiefSmokeRendererEvent& SourceEvent : State.Volume.SourceEvents)
	{
		AdvectionEvents.Add(SourceEvent);
	}

	for (const FTimeThiefSmokeRendererEvent& Event : AdvectionEvents)
	{
		if (Event.Type == ETimeThiefSmokeRendererInteractionType::BulletWake)
		{
			++BulletEventCount;
		}
	}
	BulletEventCount = FMath::Min(BulletEventCount, MaxSimulationBulletEventCount);
	int32 AdvectionEventCount = 0;
	FRDGBufferRef AdvectionEventBuffer = CreatePrioritizedSmokeEventBuffer(
		GraphBuilder,
		AdvectionEvents,
		MaxShaderEventCount,
		AdvectionEventCount,
		TEXT("TimeThiefSmoke.AdvectionEvents"));
	int32 ExplosionEventCount = 0;
	FRDGBufferRef ExplosionEventBuffer = CreateTypedSmokeEventBuffer(
		GraphBuilder,
		State.PendingEvents,
		ETimeThiefSmokeRendererInteractionType::ExplosionShock,
		MaxSimulationExplosionEventCount,
		ExplosionEventCount,
		TEXT("TimeThiefSmoke.ExplosionEvents"));
	int32 ActorEventCount = 0;
	FRDGBufferRef ActorEventBuffer = CreateTypedSmokeEventBuffer(
		GraphBuilder,
		State.PendingEvents,
		ETimeThiefSmokeRendererInteractionType::ActorPush,
		MaxSimulationActorEventCount,
		ActorEventCount,
		TEXT("TimeThiefSmoke.ActorEvents"));
	int32 ForceEventCount = 0;
	FRDGBufferRef ForceEventBuffer = CreatePrioritizedSmokeEventBuffer(
		GraphBuilder,
		State.PendingEvents,
		MaxSimulationForceEventCount,
		ForceEventCount,
		TEXT("TimeThiefSmoke.ForceEvents"));
	int32 SparseMaskEventCount = 0;
	FRDGBufferRef SparseMaskEventBuffer = CreatePrioritizedSmokeEventBuffer(
		GraphBuilder,
		AdvectionEvents,
		MaxShaderEventCount,
		SparseMaskEventCount,
		TEXT("TimeThiefSmoke.SparseMaskEvents"));
	const bool bNeedsInitThisFrame = State.bNeedsInit;
	const bool bHasExplosionEvent = ExplosionEventCount > 0;
	const bool bHasActorEvent = ActorEventCount > 0;
	const bool bHasSimulationEvent = AdvectionEventCount > 0 || ExplosionEventCount > 0 || ActorEventCount > 0 || ForceEventCount > 0;
	State.bUseSparseSimulationMaskThisFrame = State.Volume.Settings.SimulationBackend == ETimeThiefSmokeSimulationBackend::SparseMac &&
		!bNeedsInitThisFrame &&
		(State.LastActiveBrickCount > 0u || bHasSimulationEvent) &&
		State.AllocatedBrickGridSize != FIntVector(1, 1, 1);
	FRDGBufferRef ExplosionEventBrickMasksBuffer = AddBuildEventBrickMasksPass(
		GraphBuilder,
		State,
		ExplosionEventBuffer,
		ExplosionEventCount,
		TEXT("TimeThiefSmoke.ExplosionEventBrickMasks"));
	FRDGBufferRef ActorEventBrickMasksBuffer = AddBuildEventBrickMasksPass(
		GraphBuilder,
		State,
		ActorEventBuffer,
		ActorEventCount,
		TEXT("TimeThiefSmoke.ActorEventBrickMasks"));
	FRDGBufferRef ForceEventBrickMasksBuffer = AddBuildEventBrickMasksPass(
		GraphBuilder,
		State,
		ForceEventBuffer,
		ForceEventCount,
		TEXT("TimeThiefSmoke.ForceEventBrickMasks"));
	if (State.bUseSparseSimulationMaskThisFrame)
	{
		FRDGTextureRef BrickOccupancyTexture = GraphBuilder.RegisterExternalTexture(State.BrickOccupancyTexture);
		FRDGTextureRef BrickActivityTexture = CreateTransientUIntTexture(
			GraphBuilder,
			State.AllocatedBrickGridSize,
			TEXT("TimeThiefSmoke.PreEventBrickActivity"));
		AddBuildBrickOccupancyPass(
			GraphBuilder,
			State,
			DensityTextures[State.CurrentDensityIndex],
			VelocityTextures[State.CurrentVelocityIndex],
			WarpTextures[State.CurrentWarpIndex],
			BulletCutoutTextures[State.CurrentBulletFieldIndex],
			BulletSinkTextures[State.CurrentBulletFieldIndex],
			SparseMaskEventBuffer,
			SparseMaskEventCount,
			BrickActivityTexture);
		AddExpandBrickOccupancyPass(
			GraphBuilder,
			State,
			BrickActivityTexture,
			BrickOccupancyTexture,
			false);
	}
	if (bHasActorEvent)
	{
		const float WarpKeepAliveSeconds = FMath::Clamp(3.0f / FMath::Max(State.Volume.Settings.WarpTrailDecayRate, 0.25f), 0.5f, 4.0f);
		State.WarpDecayBudgetSeconds = FMath::Max(State.WarpDecayBudgetSeconds, WarpKeepAliveSeconds);
		State.bWarpClearPending = false;
	}
	const bool bRunWarpPass = bHasActorEvent || State.WarpDecayBudgetSeconds > UE_SMALL_NUMBER;
	const bool bVortexUploadPending = State.bVortexParticlesNeedUpload;
	State.AccumulatedVortexDeltaSeconds = FMath::Min(State.AccumulatedVortexDeltaSeconds + DeltaSeconds, VortexSubstepIntervalSeconds * 2.0f);
	const bool bRunVortexPasses = bNeedsInitThisFrame || bVortexUploadPending || bHasSimulationEvent || State.AccumulatedVortexDeltaSeconds >= VortexSubstepIntervalSeconds;
	int32 ProfilePassCount = 0;
	ProfilePassCount += BulletEventCount > 0 ? 1 : 0;
	ProfilePassCount += ExplosionEventCount > 0 ? 1 : 0;
	ProfilePassCount += ActorEventCount > 0 ? 1 : 0;
	ProfilePassCount += ForceEventCount > 0 ? 1 : 0;
	ProfilePassCount += State.bUseSparseSimulationMaskThisFrame ? 2 : 0;
	ProfilePassCount += bNeedsInitThisFrame ? 7 : 0;
	ProfilePassCount += 1;
	ProfilePassCount += bRunWarpPass ? 1 : (State.bWarpClearPending ? 1 : 0);
	ProfilePassCount += bHasExplosionEvent ? 1 : 0;
	ProfilePassCount += bHasActorEvent ? 1 : 0;
	ProfilePassCount += bRunVortexPasses ? 4 + (State.Volume.Settings.bUseVortexBrickBins ? 1 : 0) : 0;
	ProfilePassCount += bUseMacProjection ? 2 : 1;
	ProfilePassCount += State.Volume.Settings.PressureSolver == ETimeThiefSmokePressureSolver::Multigrid
		? EstimateMultigridPassCount(State.AllocatedGridSize)
		: FMath::Clamp(State.Volume.Settings.PressureIterations, 1, 64);
	ProfilePassCount += bUseMacProjection ? 2 : 1;
	ProfilePassCount += State.Volume.Settings.SimulationBackend == ETimeThiefSmokeSimulationBackend::SparseMac
		? (State.bForceDenseComposite ? 2 : 3)
		: 0;
	State.LastProfilePassCount = ProfilePassCount;

	uint64 EstimatedVRAMBytes = 0;
	EstimatedVRAMBytes += EstimateTextureBytes(State.AllocatedGridSize, 2) * 2;
	EstimatedVRAMBytes += EstimateTextureBytes(State.AllocatedGridSize, 16) * 2;
	EstimatedVRAMBytes += EstimateTextureBytes(State.AllocatedGridSize, 2) * 2;
	EstimatedVRAMBytes += EstimateTextureBytes(State.AllocatedGridSize, 2);
	EstimatedVRAMBytes += EstimateTextureBytes(State.AllocatedGridSize, 2) * 4;
	EstimatedVRAMBytes += EstimateTextureBytes(State.AllocatedGridSize, 2) * 2;
	EstimatedVRAMBytes += EstimateTextureBytes(State.AllocatedGridSize, 16);
	if (bUseMacProjection)
	{
		EstimatedVRAMBytes += EstimateTextureBytes(State.AllocatedGridSize, 2) * 6;
	}
	EstimatedVRAMBytes += EstimateTextureBytes(State.AllocatedBrickGridSize, 4);
	EstimatedVRAMBytes += EstimateTextureBytes(State.AllocatedSparseAtlasGridSize, 16);
	const int32 ObstacleResolution = FMath::Max(State.AllocatedObstacleResolution, 1);
	EstimatedVRAMBytes += static_cast<uint64>(ObstacleResolution) * ObstacleResolution * ObstacleResolution;
	EstimatedVRAMBytes += static_cast<uint64>(FMath::Max(State.AllocatedVortexParticleCount, 1)) * sizeof(FTimeThiefSmokeVortexParticleShaderData) * 2;
	State.LastEstimatedVRAMBytes = EstimatedVRAMBytes;

	if (State.bVortexParticlesNeedUpload)
	{
		UploadDeadVortexParticles(GraphBuilder, State, VortexParticleBuffers[0]);
		UploadDeadVortexParticles(GraphBuilder, State, VortexParticleBuffers[1]);
		State.CurrentVortexParticleIndex = 0;
		State.bVortexParticlesNeedUpload = false;
	}

	if (State.bNeedsInit)
	{
		AddInitPass(GraphBuilder, State, DensityTextures[State.CurrentDensityIndex], VelocityTextures[State.CurrentVelocityIndex]);
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(BulletCutoutTextures[0]), 0.0f);
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(BulletCutoutTextures[1]), 0.0f);
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(BulletSinkTextures[0]), 0.0f);
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(BulletSinkTextures[1]), 0.0f);
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(WarpTextures[0]), 0.0f);
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(WarpTextures[1]), 0.0f);
		if (!bHasActorEvent)
		{
			State.WarpDecayBudgetSeconds = 0.0f;
			State.bWarpClearPending = false;
		}
		State.bNeedsInit = false;
	}

	const int32 ReadDensityIndex = State.CurrentDensityIndex;
	const int32 ReadVelocityIndex = State.CurrentVelocityIndex;
	const int32 WriteDensityIndex = 1 - State.CurrentDensityIndex;
	const int32 WriteVelocityIndex = 1 - State.CurrentVelocityIndex;

	const int32 BulletReadIndex = State.CurrentBulletFieldIndex;
	const int32 BulletWriteIndex = 1 - State.CurrentBulletFieldIndex;
	AddSimulatePass(
		GraphBuilder,
		State,
		DensityTextures[ReadDensityIndex],
		VelocityTextures[ReadVelocityIndex],
		BulletCutoutTextures[BulletReadIndex],
		BulletSinkTextures[BulletReadIndex],
		AdvectionEventBuffer,
		AddBuildEventBrickMasksPass(
			GraphBuilder,
			State,
			AdvectionEventBuffer,
			AdvectionEventCount,
			TEXT("TimeThiefSmoke.AdvectionEventBrickMasks")),
		AdvectionEventCount,
		DensityTextures[WriteDensityIndex],
		VelocityTextures[WriteVelocityIndex],
		BulletCutoutTextures[BulletWriteIndex],
		BulletSinkTextures[BulletWriteIndex],
		DeltaSeconds);
	State.CurrentDensityIndex = WriteDensityIndex;
	State.CurrentVelocityIndex = WriteVelocityIndex;
	State.CurrentBulletFieldIndex = BulletWriteIndex;

	if (bRunWarpPass)
	{
		const int32 WarpReadIndex = State.CurrentWarpIndex;
		const int32 WarpWriteIndex = 1 - State.CurrentWarpIndex;
		AddWarpPass(GraphBuilder, State, DensityTextures[State.CurrentDensityIndex], WarpTextures[WarpReadIndex], WarpTextures[WarpWriteIndex], ActorEventBuffer, ActorEventBrickMasksBuffer, ActorEventCount, DeltaSeconds);
		State.CurrentWarpIndex = WarpWriteIndex;
		State.WarpDecayBudgetSeconds = FMath::Max(0.0f, State.WarpDecayBudgetSeconds - DeltaSeconds);
		State.bWarpClearPending = !bHasActorEvent && State.WarpDecayBudgetSeconds <= UE_SMALL_NUMBER;
	}
	else if (State.bWarpClearPending)
	{
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(WarpTextures[State.CurrentWarpIndex]), 0.0f);
		State.bWarpClearPending = false;
	}

	if (bHasExplosionEvent || bHasActorEvent)
	{
		if (bHasExplosionEvent)
		{
			const int32 EventReadDensityIndex = State.CurrentDensityIndex;
			const int32 EventReadVelocityIndex = State.CurrentVelocityIndex;
			const int32 EventWriteDensityIndex = 1 - State.CurrentDensityIndex;
			const int32 EventWriteVelocityIndex = 1 - State.CurrentVelocityIndex;
			AddApplyEventsPass(GraphBuilder, State, DensityTextures[EventReadDensityIndex], VelocityTextures[EventReadVelocityIndex], DensityTextures[EventWriteDensityIndex], VelocityTextures[EventWriteVelocityIndex], ExplosionEventBuffer, ExplosionEventBrickMasksBuffer, ExplosionEventCount, DeltaSeconds);
			State.CurrentDensityIndex = EventWriteDensityIndex;
			State.CurrentVelocityIndex = EventWriteVelocityIndex;
		}

		if (bHasActorEvent)
		{
			const int32 ObstacleReadDensityIndex = State.CurrentDensityIndex;
			const int32 ObstacleReadVelocityIndex = State.CurrentVelocityIndex;
			const int32 ObstacleWriteDensityIndex = 1 - State.CurrentDensityIndex;
			const int32 ObstacleWriteVelocityIndex = 1 - State.CurrentVelocityIndex;
			AddDynamicObstaclePass(GraphBuilder, State, DensityTextures[ObstacleReadDensityIndex], VelocityTextures[ObstacleReadVelocityIndex], DensityTextures[ObstacleWriteDensityIndex], VelocityTextures[ObstacleWriteVelocityIndex], ActorEventBuffer, ActorEventBrickMasksBuffer, ActorEventCount, DeltaSeconds);
			State.CurrentDensityIndex = ObstacleWriteDensityIndex;
			State.CurrentVelocityIndex = ObstacleWriteVelocityIndex;
		}
	}

	if (bRunVortexPasses)
	{
		const float VortexDeltaSeconds = FMath::Max(State.AccumulatedVortexDeltaSeconds, DeltaSeconds);
		const int32 VortexParticleReadIndex = State.CurrentVortexParticleIndex;
		const int32 VortexParticleWriteIndex = 1 - State.CurrentVortexParticleIndex;
		AddVortexParticleUpdatePass(
			GraphBuilder,
			State,
			VortexParticleBuffers[VortexParticleReadIndex],
			VortexParticleBuffers[VortexParticleWriteIndex],
			DensityTextures[State.CurrentDensityIndex],
			VelocityTextures[State.CurrentVelocityIndex],
			BulletCutoutTextures[State.CurrentBulletFieldIndex],
			BulletSinkTextures[State.CurrentBulletFieldIndex],
			ForceEventBuffer,
			ForceEventCount,
			VortexDeltaSeconds);
		State.CurrentVortexParticleIndex = VortexParticleWriteIndex;

		const int32 VorticityWriteVelocityIndex = 1 - State.CurrentVelocityIndex;
		AddBuildCurlPass(GraphBuilder, State, VelocityTextures[State.CurrentVelocityIndex], CurlTexture);
		AddVorticityPass(GraphBuilder, State, DensityTextures[State.CurrentDensityIndex], VelocityTextures[State.CurrentVelocityIndex], CurlTexture, VelocityTextures[VorticityWriteVelocityIndex], ForceEventBuffer, ForceEventBrickMasksBuffer, ForceEventCount, VortexDeltaSeconds);
		State.CurrentVelocityIndex = VorticityWriteVelocityIndex;

		const int32 VortexSplatWriteVelocityIndex = 1 - State.CurrentVelocityIndex;
		FRDGBufferRef VortexBrickMasksBuffer = nullptr;
		if (State.Volume.Settings.bUseVortexBrickBins)
		{
			VortexBrickMasksBuffer = AddBuildVortexBrickMasksPass(
				GraphBuilder,
				State,
				VortexParticleBuffers[State.CurrentVortexParticleIndex]);
		}
		else
		{
			TArray<uint32> EmptyMask;
			EmptyMask.AddZeroed(4);
			VortexBrickMasksBuffer = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32) * 4, 1),
				TEXT("TimeThiefSmoke.EmptyVortexBrickMasks"));
			GraphBuilder.QueueBufferUpload(VortexBrickMasksBuffer, EmptyMask.GetData(), EmptyMask.Num() * sizeof(uint32));
		}
		AddVortexParticleSplatPass(
			GraphBuilder,
			State,
			VortexParticleBuffers[State.CurrentVortexParticleIndex],
			VortexBrickMasksBuffer,
			DensityTextures[State.CurrentDensityIndex],
			VelocityTextures[State.CurrentVelocityIndex],
			BulletCutoutTextures[State.CurrentBulletFieldIndex],
			BulletSinkTextures[State.CurrentBulletFieldIndex],
			VelocityTextures[VortexSplatWriteVelocityIndex]);
		State.CurrentVelocityIndex = VortexSplatWriteVelocityIndex;
		State.AccumulatedVortexDeltaSeconds = 0.0f;
	}
	State.PendingEvents.Reset();

	if (bUseMacProjection)
	{
		AddBuildMacVelocityPass(
			GraphBuilder,
			State,
			VelocityTextures[State.CurrentVelocityIndex],
			MacVelocityUTextures[0],
			MacVelocityVTextures[0],
			MacVelocityWTextures[0]);
		AddMacDivergencePass(
			GraphBuilder,
			State,
			MacVelocityUTextures[0],
			MacVelocityVTextures[0],
			MacVelocityWTextures[0],
			DivergenceTexture,
			PressureTextures[0]);
	}
	else
	{
		AddDivergencePass(GraphBuilder, State, VelocityTextures[State.CurrentVelocityIndex], DivergenceTexture, PressureTextures[0]);
	}

	FRDGTextureRef PressureForProjection = PressureTextures[0];
	if (State.Volume.Settings.PressureSolver == ETimeThiefSmokePressureSolver::Multigrid)
	{
		PressureForProjection = AddPressureSolvePass(GraphBuilder, State, DivergenceTexture, ObstacleTexture, PressureTextures[0], PressureTextures[1]);
	}
	else
	{
		const FVector3f CellSize = MakeCellSize(State.Volume, State.AllocatedGridSize);
		int32 CurrentPressureIndex = 0;
		const int32 PressureIterations = FMath::Clamp(State.Volume.Settings.PressureIterations, 1, 64);
		for (int32 Iteration = 0; Iteration < PressureIterations; ++Iteration)
		{
			const int32 NextPressureIndex = 1 - CurrentPressureIndex;
			AddPressureJacobiPass(GraphBuilder, State, State.AllocatedGridSize, CellSize, PressureTextures[CurrentPressureIndex], DivergenceTexture, ObstacleTexture, PressureTextures[NextPressureIndex]);
			CurrentPressureIndex = NextPressureIndex;
		}
		PressureForProjection = PressureTextures[CurrentPressureIndex];
	}

	const int32 ProjectedVelocityIndex = 1 - State.CurrentVelocityIndex;
	if (bUseMacProjection)
	{
		AddProjectMacVelocityPass(
			GraphBuilder,
			State,
			MacVelocityUTextures[0],
			MacVelocityVTextures[0],
			MacVelocityWTextures[0],
			PressureForProjection,
			MacVelocityUTextures[1],
			MacVelocityVTextures[1],
			MacVelocityWTextures[1]);
		AddMacToCollocatedVelocityPass(
			GraphBuilder,
			State,
			MacVelocityUTextures[1],
			MacVelocityVTextures[1],
			MacVelocityWTextures[1],
			VelocityTextures[ProjectedVelocityIndex]);
	}
	else
	{
		AddProjectVelocityPass(GraphBuilder, State, VelocityTextures[State.CurrentVelocityIndex], PressureForProjection, VelocityTextures[ProjectedVelocityIndex]);
	}
	State.CurrentVelocityIndex = ProjectedVelocityIndex;

	if (State.Volume.Settings.SimulationBackend == ETimeThiefSmokeSimulationBackend::SparseMac)
	{
		FRDGTextureRef BrickOccupancyTexture = GraphBuilder.RegisterExternalTexture(State.BrickOccupancyTexture);
		FRDGTextureRef BrickActivityTexture = CreateTransientUIntTexture(
			GraphBuilder,
			State.AllocatedBrickGridSize,
			TEXT("TimeThiefSmoke.BrickActivity"));
		AddBuildBrickOccupancyPass(
			GraphBuilder,
			State,
			DensityTextures[State.CurrentDensityIndex],
			VelocityTextures[State.CurrentVelocityIndex],
			WarpTextures[State.CurrentWarpIndex],
			BulletCutoutTextures[State.CurrentBulletFieldIndex],
			BulletSinkTextures[State.CurrentBulletFieldIndex],
			SparseMaskEventBuffer,
			SparseMaskEventCount,
			BrickActivityTexture);
		const FActiveBrickDispatchResources ActiveBrickResources = AddExpandBrickOccupancyPass(
			GraphBuilder,
			State,
			BrickActivityTexture,
			BrickOccupancyTexture,
			true);
		if (!State.bForceDenseComposite)
		{
			AddScatterSparseAtlasPass(
				GraphBuilder,
				State,
				DensityTextures[State.CurrentDensityIndex],
				WarpTextures[State.CurrentWarpIndex],
				BulletCutoutTextures[State.CurrentBulletFieldIndex],
				BulletSinkTextures[State.CurrentBulletFieldIndex],
				ActiveBrickResources);
		}
	}
}

void FTimeThiefSmokeViewExtension::AddInitPass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGTextureRef DensityTexture,
	FRDGTextureRef VelocityTexture)
{
	const FIntVector GridSize = State.AllocatedGridSize;
	const FIntVector GroupCount = MakeGroupCount(GridSize);

	TShaderMapRef<FTimeThiefSmokeInitCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokeInitCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeInitCS::FParameters>();
	PassParameters->OutDensity = GraphBuilder.CreateUAV(DensityTexture);
	PassParameters->OutVelocity = GraphBuilder.CreateUAV(VelocityTexture);
	PassParameters->GridResolution = GridSize;
	PassParameters->BoundsExtent = FVector3f(State.Volume.BoundsExtent);
	PassParameters->NaturalBoundsExtent = FVector3f(State.Volume.NaturalBoundsExtent);
	PassParameters->InitialDensity = State.Volume.SourceEvents.Num() > 0 ? 0.0f : State.Volume.Settings.InitialDensity;
	PassParameters->PlumeSourceRadius = State.Volume.Settings.PlumeSourceRadius;

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.Init SmokeId=%d", State.Volume.SmokeId),
		ComputeShader,
		PassParameters,
		GroupCount);
}

void FTimeThiefSmokeViewExtension::AddApplyEventsPass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGTextureRef DensityIn,
	FRDGTextureRef VelocityIn,
	FRDGTextureRef DensityOut,
	FRDGTextureRef VelocityOut,
	FRDGBufferRef EventBuffer,
	FRDGBufferRef EventBrickMasksBuffer,
	int32 EventCount,
	float DeltaSeconds)
{
	const FIntVector GridSize = State.AllocatedGridSize;
	const FIntVector GroupCount = MakeGroupCount(GridSize);

	TShaderMapRef<FTimeThiefSmokeApplyEventsCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokeApplyEventsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeApplyEventsCS::FParameters>();
	PassParameters->DensityIn = DensityIn;
	PassParameters->VelocityIn = VelocityIn;
	PassParameters->ObstacleTexture = GraphBuilder.RegisterExternalTexture(State.ObstacleTexture);
	PassParameters->BrickOccupancyTexture = GraphBuilder.RegisterExternalTexture(State.BrickOccupancyTexture);
	PassParameters->VolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->Events = GraphBuilder.CreateSRV(EventBuffer);
	PassParameters->EventBrickMasks = GraphBuilder.CreateSRV(EventBrickMasksBuffer);
	PassParameters->OutDensity = GraphBuilder.CreateUAV(DensityOut);
	PassParameters->OutVelocity = GraphBuilder.CreateUAV(VelocityOut);
	PassParameters->GridResolution = GridSize;
	PassParameters->BrickGridResolution = State.AllocatedBrickGridSize;
	PassParameters->SmokeBrickSize = FMath::Clamp(State.Volume.Settings.SmokeBrickSize, 4, 32);
	PassParameters->bUseSparseSimulationMask = State.bUseSparseSimulationMaskThisFrame ? 1u : 0u;
	PassParameters->bUseEventBrickBins = EventCount > 0 ? 1u : 0u;
	PassParameters->BoundsExtent = FVector3f(State.Volume.BoundsExtent);
	PassParameters->DeltaSeconds = DeltaSeconds;
	PassParameters->EventCount = EventCount;
	PassParameters->LocalToWorld = State.Volume.LocalToWorld.ToMatrixWithScale();
	PassParameters->WorldToLocal = State.Volume.LocalToWorld.ToInverseMatrixWithScale();

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.ApplyEvents SmokeId=%d Events=%d", State.Volume.SmokeId, EventCount),
		ComputeShader,
		PassParameters,
		GroupCount);
}

void FTimeThiefSmokeViewExtension::AddDynamicObstaclePass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGTextureRef DensityIn,
	FRDGTextureRef VelocityIn,
	FRDGTextureRef DensityOut,
	FRDGTextureRef VelocityOut,
	FRDGBufferRef EventBuffer,
	FRDGBufferRef EventBrickMasksBuffer,
	int32 EventCount,
	float DeltaSeconds)
{
	const FIntVector GridSize = State.AllocatedGridSize;
	const FIntVector GroupCount = MakeGroupCount(GridSize);

	TShaderMapRef<FTimeThiefSmokeDynamicObstacleCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokeDynamicObstacleCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeDynamicObstacleCS::FParameters>();
	PassParameters->DensityIn = DensityIn;
	PassParameters->VelocityIn = VelocityIn;
	PassParameters->ObstacleTexture = GraphBuilder.RegisterExternalTexture(State.ObstacleTexture);
	PassParameters->BrickOccupancyTexture = GraphBuilder.RegisterExternalTexture(State.BrickOccupancyTexture);
	PassParameters->VolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->Events = GraphBuilder.CreateSRV(EventBuffer);
	PassParameters->EventBrickMasks = GraphBuilder.CreateSRV(EventBrickMasksBuffer);
	PassParameters->OutDensity = GraphBuilder.CreateUAV(DensityOut);
	PassParameters->OutVelocity = GraphBuilder.CreateUAV(VelocityOut);
	PassParameters->GridResolution = GridSize;
	PassParameters->BrickGridResolution = State.AllocatedBrickGridSize;
	PassParameters->SmokeBrickSize = FMath::Clamp(State.Volume.Settings.SmokeBrickSize, 4, 32);
	PassParameters->bUseSparseSimulationMask = State.bUseSparseSimulationMaskThisFrame ? 1u : 0u;
	PassParameters->bUseEventBrickBins = EventCount > 0 ? 1u : 0u;
	PassParameters->BoundsExtent = FVector3f(State.Volume.BoundsExtent);
	PassParameters->DeltaSeconds = DeltaSeconds;
	PassParameters->ActorAirflowStrength = FMath::Max(0.0f, State.Volume.Settings.ActorAirflowStrength);
	PassParameters->ActorAirflowMinSpeed = FMath::Max(0.0f, State.Volume.Settings.ActorAirflowMinSpeed);
	PassParameters->ActorAirflowFullSpeed = FMath::Max(PassParameters->ActorAirflowMinSpeed + 1.0f, State.Volume.Settings.ActorAirflowFullSpeed);
	PassParameters->ActorAirflowRadiusScale = FMath::Max(0.1f, State.Volume.Settings.ActorAirflowRadiusScale);
	PassParameters->ActorAirflowFrontStrength = FMath::Max(0.0f, State.Volume.Settings.ActorAirflowFrontStrength);
	PassParameters->ActorAirflowSideStrength = FMath::Max(0.0f, State.Volume.Settings.ActorAirflowSideStrength);
	PassParameters->ActorAirflowWakeStrength = FMath::Max(0.0f, State.Volume.Settings.ActorAirflowWakeStrength);
	PassParameters->EventCount = EventCount;
	PassParameters->LocalToWorld = State.Volume.LocalToWorld.ToMatrixWithScale();
	PassParameters->WorldToLocal = State.Volume.LocalToWorld.ToInverseMatrixWithScale();

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.DynamicObstacle SmokeId=%d Events=%d", State.Volume.SmokeId, EventCount),
		ComputeShader,
		PassParameters,
		GroupCount);
}

void FTimeThiefSmokeViewExtension::AddSimulatePass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGTextureRef DensityIn,
	FRDGTextureRef VelocityIn,
	FRDGTextureRef BulletCutoutTexture,
	FRDGTextureRef BulletSinkTexture,
	FRDGBufferRef EventBuffer,
	FRDGBufferRef EventBrickMasksBuffer,
	int32 EventCount,
	FRDGTextureRef DensityOut,
	FRDGTextureRef VelocityOut,
	FRDGTextureRef BulletCutoutOut,
	FRDGTextureRef BulletSinkOut,
	float DeltaSeconds)
{
	const FIntVector GridSize = State.AllocatedGridSize;
	const FIntVector GroupCount = MakeGroupCount(GridSize);

	TShaderMapRef<FTimeThiefSmokeSimulateCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokeSimulateCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeSimulateCS::FParameters>();
	PassParameters->DensityIn = DensityIn;
	PassParameters->VelocityIn = VelocityIn;
	PassParameters->ObstacleTexture = GraphBuilder.RegisterExternalTexture(State.ObstacleTexture);
	PassParameters->BrickOccupancyTexture = GraphBuilder.RegisterExternalTexture(State.BrickOccupancyTexture);
	PassParameters->BulletCutoutTexture = BulletCutoutTexture;
	PassParameters->BulletSinkTexture = BulletSinkTexture;
	PassParameters->VolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->Events = GraphBuilder.CreateSRV(EventBuffer);
	PassParameters->EventBrickMasks = GraphBuilder.CreateSRV(EventBrickMasksBuffer);
	PassParameters->OutDensity = GraphBuilder.CreateUAV(DensityOut);
	PassParameters->OutVelocity = GraphBuilder.CreateUAV(VelocityOut);
	PassParameters->OutCutout = GraphBuilder.CreateUAV(BulletCutoutOut);
	PassParameters->OutSink = GraphBuilder.CreateUAV(BulletSinkOut);
	PassParameters->GridResolution = GridSize;
	PassParameters->BrickGridResolution = State.AllocatedBrickGridSize;
	PassParameters->SmokeBrickSize = FMath::Clamp(State.Volume.Settings.SmokeBrickSize, 4, 32);
	PassParameters->bUseSparseSimulationMask = State.bUseSparseSimulationMaskThisFrame ? 1u : 0u;
	PassParameters->bUseEventBrickBins = EventCount > 0 ? 1u : 0u;
	PassParameters->bUseClusterSourceEvents = State.Volume.SourceEvents.Num() > 0 ? 1u : 0u;
	PassParameters->BoundsExtent = FVector3f(State.Volume.BoundsExtent);
	PassParameters->NaturalBoundsExtent = FVector3f(State.Volume.NaturalBoundsExtent);
	PassParameters->DeltaSeconds = DeltaSeconds;
	PassParameters->InitialDensity = State.Volume.Settings.InitialDensity;
	PassParameters->AgeSeconds = State.Volume.AgeSeconds;
	PassParameters->DurationSeconds = State.Volume.DurationSeconds;
	PassParameters->SmokeFadeOutDuration = State.Volume.Settings.SmokeFadeOutDuration;
	PassParameters->PlumeEmissionDuration = State.Volume.Settings.PlumeEmissionDuration;
	PassParameters->PlumeSourceRadius = State.Volume.Settings.PlumeSourceRadius;
	PassParameters->PlumeExpansionVelocity = State.Volume.Settings.PlumeExpansionVelocity;
	PassParameters->PlumeRiseVelocity = State.Volume.Settings.PlumeRiseVelocity;
	PassParameters->DensityDissipation = State.Volume.Settings.DensityDissipation;
	PassParameters->VelocityDamping = State.Volume.Settings.VelocityDamping;
	PassParameters->VorticityStrength = State.Volume.Settings.VorticityStrength;
	PassParameters->SelfWobbleTimeScale = State.Volume.Settings.SelfWobbleTimeScale;
	PassParameters->SelfWobbleVelocityScale = State.Volume.Settings.SelfWobbleVelocityScale;
	PassParameters->bUseMacCormackAdvection = State.Volume.Settings.bUseMacCormackAdvection ? 1u : 0u;
	PassParameters->bUseAdaptiveMacCormack = State.Volume.Settings.bUseAdaptiveMacCormack ? 1u : 0u;
	PassParameters->EventCount = EventCount;
	PassParameters->BulletWakeCutoutLife = FMath::Max(0.05f, State.Volume.Settings.BulletWakeMaxVisibleLife);
	PassParameters->BulletWakeReleaseDuration = FMath::Max(0.05f, State.Volume.Settings.BulletWakeReleaseDuration);
	PassParameters->BulletWakeSinkLife = FMath::Max(0.05f, State.Volume.Settings.BulletWakeSinkLife);
	PassParameters->BulletWakeSinkStrength = FMath::Max(0.0f, State.Volume.Settings.BulletWakeSinkStrength);
	PassParameters->BulletWakeImpulseStrength = State.Volume.Settings.BulletWakeImpulseStrength;
	PassParameters->BulletWakeCutoutFeather = FMath::Max(0.2f, State.Volume.Settings.BulletWakeCutoutFeather);
	PassParameters->LocalToWorld = State.Volume.LocalToWorld.ToMatrixWithScale();
	PassParameters->WorldToLocal = State.Volume.LocalToWorld.ToInverseMatrixWithScale();

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.Simulate SmokeId=%d", State.Volume.SmokeId),
		ComputeShader,
		PassParameters,
		GroupCount);
}

void FTimeThiefSmokeViewExtension::AddWarpPass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGTextureRef DensityIn,
	FRDGTextureRef WarpIn,
	FRDGTextureRef WarpOut,
	FRDGBufferRef EventBuffer,
	FRDGBufferRef EventBrickMasksBuffer,
	int32 EventCount,
	float DeltaSeconds)
{
	const FIntVector GridSize = State.AllocatedGridSize;
	const FIntVector GroupCount = MakeGroupCount(GridSize);

	TShaderMapRef<FTimeThiefSmokeWarpCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokeWarpCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeWarpCS::FParameters>();
	PassParameters->DensityIn = DensityIn;
	PassParameters->WarpIn = WarpIn;
	PassParameters->ObstacleTexture = GraphBuilder.RegisterExternalTexture(State.ObstacleTexture);
	PassParameters->BrickOccupancyTexture = GraphBuilder.RegisterExternalTexture(State.BrickOccupancyTexture);
	PassParameters->VolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->Events = GraphBuilder.CreateSRV(EventBuffer);
	PassParameters->EventBrickMasks = GraphBuilder.CreateSRV(EventBrickMasksBuffer);
	PassParameters->OutWarp = GraphBuilder.CreateUAV(WarpOut);
	PassParameters->GridResolution = GridSize;
	PassParameters->BrickGridResolution = State.AllocatedBrickGridSize;
	PassParameters->SmokeBrickSize = FMath::Clamp(State.Volume.Settings.SmokeBrickSize, 4, 32);
	PassParameters->bUseSparseSimulationMask = State.bUseSparseSimulationMaskThisFrame ? 1u : 0u;
	PassParameters->bUseEventBrickBins = EventCount > 0 ? 1u : 0u;
	PassParameters->BoundsExtent = FVector3f(State.Volume.BoundsExtent);
	PassParameters->DeltaSeconds = DeltaSeconds;
	PassParameters->WarpTrailIntensity = State.Volume.Settings.WarpTrailIntensity;
	PassParameters->WarpTrailDecayRate = State.Volume.Settings.WarpTrailDecayRate;
	PassParameters->WarpTrailRadiusScale = State.Volume.Settings.WarpTrailRadiusScale;
	PassParameters->WarpTrailLengthScale = State.Volume.Settings.WarpTrailLengthScale;
	PassParameters->EventCount = EventCount;
	PassParameters->LocalToWorld = State.Volume.LocalToWorld.ToMatrixWithScale();

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.Warp SmokeId=%d Events=%d", State.Volume.SmokeId, EventCount),
		ComputeShader,
		PassParameters,
		GroupCount);
}

void FTimeThiefSmokeViewExtension::AddBuildCurlPass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGTextureRef VelocityIn,
	FRDGTextureRef CurlOut)
{
	const FIntVector GridSize = State.AllocatedGridSize;
	const FIntVector GroupCount = MakeGroupCount(GridSize);
	const FVector3f CellSize = MakeCellSize(State.Volume, GridSize);

	TShaderMapRef<FTimeThiefSmokeBuildCurlCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokeBuildCurlCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeBuildCurlCS::FParameters>();
	PassParameters->GridResolution = GridSize;
	PassParameters->CellSize = CellSize;
	PassParameters->VelocityIn = VelocityIn;
	PassParameters->ObstacleTexture = GraphBuilder.RegisterExternalTexture(State.ObstacleTexture);
	PassParameters->BrickOccupancyTexture = GraphBuilder.RegisterExternalTexture(State.BrickOccupancyTexture);
	PassParameters->VolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->OutCurl = GraphBuilder.CreateUAV(CurlOut);
	PassParameters->BrickGridResolution = State.AllocatedBrickGridSize;
	PassParameters->SmokeBrickSize = FMath::Clamp(State.Volume.Settings.SmokeBrickSize, 4, 32);
	PassParameters->bUseSparseSimulationMask = State.bUseSparseSimulationMaskThisFrame ? 1u : 0u;

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.BuildCurl SmokeId=%d", State.Volume.SmokeId),
		ComputeShader,
		PassParameters,
		GroupCount);
}

void FTimeThiefSmokeViewExtension::AddVorticityPass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGTextureRef DensityIn,
	FRDGTextureRef VelocityIn,
	FRDGTextureRef CurlTexture,
	FRDGTextureRef VelocityOut,
	FRDGBufferRef EventBuffer,
	FRDGBufferRef EventBrickMasksBuffer,
	int32 EventCount,
	float DeltaSeconds)
{
	const FIntVector GridSize = State.AllocatedGridSize;
	const FIntVector GroupCount = MakeGroupCount(GridSize);
	const FVector3f CellSize = MakeCellSize(State.Volume, GridSize);

	TShaderMapRef<FTimeThiefSmokeVorticityCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokeVorticityCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeVorticityCS::FParameters>();
	PassParameters->DensityIn = DensityIn;
	PassParameters->VelocityIn = VelocityIn;
	PassParameters->CurlTexture = CurlTexture;
	PassParameters->ObstacleTexture = GraphBuilder.RegisterExternalTexture(State.ObstacleTexture);
	PassParameters->BrickOccupancyTexture = GraphBuilder.RegisterExternalTexture(State.BrickOccupancyTexture);
	PassParameters->VolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->Events = GraphBuilder.CreateSRV(EventBuffer);
	PassParameters->EventBrickMasks = GraphBuilder.CreateSRV(EventBrickMasksBuffer);
	PassParameters->OutVelocity = GraphBuilder.CreateUAV(VelocityOut);
	PassParameters->GridResolution = GridSize;
	PassParameters->BrickGridResolution = State.AllocatedBrickGridSize;
	PassParameters->SmokeBrickSize = FMath::Clamp(State.Volume.Settings.SmokeBrickSize, 4, 32);
	PassParameters->bUseSparseSimulationMask = State.bUseSparseSimulationMaskThisFrame ? 1u : 0u;
	PassParameters->bUseEventBrickBins = EventCount > 0 ? 1u : 0u;
	PassParameters->BoundsExtent = FVector3f(State.Volume.BoundsExtent);
	PassParameters->CellSize = CellSize;
	PassParameters->DeltaSeconds = DeltaSeconds;
	PassParameters->AgeSeconds = State.Volume.AgeSeconds;
	PassParameters->VorticityConfinementStrength = State.Volume.Settings.VorticityConfinementStrength;
	PassParameters->TurbulenceStrength = State.Volume.Settings.TurbulenceStrength;
	PassParameters->AirInteractionStrength = State.Volume.Settings.AirInteractionStrength;
	PassParameters->SelfWobbleTimeScale = State.Volume.Settings.SelfWobbleTimeScale;
	PassParameters->SelfWobbleForceScale = State.Volume.Settings.SelfWobbleForceScale;
	PassParameters->EventVortexStrength = State.Volume.Settings.EventVortexStrength;
	PassParameters->ActorAirflowFullSpeed = FMath::Max(1.0f, State.Volume.Settings.ActorAirflowFullSpeed);
	PassParameters->ActorAirflowRadiusScale = FMath::Max(0.1f, State.Volume.Settings.ActorAirflowRadiusScale);
	PassParameters->ActorAirflowVortexStrength = FMath::Max(0.0f, State.Volume.Settings.ActorAirflowVortexStrength);
	PassParameters->EventCount = EventCount;
	PassParameters->LocalToWorld = State.Volume.LocalToWorld.ToMatrixWithScale();

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.Vorticity SmokeId=%d Events=%d", State.Volume.SmokeId, EventCount),
		ComputeShader,
		PassParameters,
		GroupCount);
}

void FTimeThiefSmokeViewExtension::AddDivergencePass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGTextureRef VelocityIn,
	FRDGTextureRef DivergenceOut,
	FRDGTextureRef PressureOut)
{
	const FIntVector GridSize = State.AllocatedGridSize;
	const FIntVector GroupCount = MakeGroupCount(GridSize);
	const FVector3f CellSize = MakeCellSize(State.Volume, GridSize);

	TShaderMapRef<FTimeThiefSmokeDivergenceCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokeDivergenceCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeDivergenceCS::FParameters>();
	PassParameters->GridResolution = GridSize;
	PassParameters->CellSize = CellSize;
	PassParameters->VelocityIn = VelocityIn;
	PassParameters->ObstacleTexture = GraphBuilder.RegisterExternalTexture(State.ObstacleTexture);
	PassParameters->VolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->OutDivergence = GraphBuilder.CreateUAV(DivergenceOut);
	PassParameters->OutPressure = GraphBuilder.CreateUAV(PressureOut);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.Divergence SmokeId=%d", State.Volume.SmokeId),
		ComputeShader,
		PassParameters,
		GroupCount);
}

void FTimeThiefSmokeViewExtension::AddBuildBrickOccupancyPass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGTextureRef DensityTexture,
	FRDGTextureRef VelocityTexture,
	FRDGTextureRef WarpTexture,
	FRDGTextureRef BulletCutoutTexture,
	FRDGTextureRef BulletSinkTexture,
	FRDGBufferRef ActorEventBuffer,
	int32 ActorEventCount,
	FRDGTextureRef BrickActivityTexture)
{
	const int32 BrickSize = FMath::Clamp(State.Volume.Settings.SmokeBrickSize, 4, 32);
	const FIntVector BrickGridSize = MakeBrickGridSize(State.AllocatedGridSize, BrickSize);
	const FIntVector GroupCount = BrickGridSize;

	TShaderMapRef<FTimeThiefSmokeBuildBrickOccupancyCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokeBuildBrickOccupancyCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeBuildBrickOccupancyCS::FParameters>();
	PassParameters->GridResolution = State.AllocatedGridSize;
	PassParameters->BrickGridResolution = BrickGridSize;
	PassParameters->SmokeBrickSize = BrickSize;
	PassParameters->BoundsExtent = FVector3f(State.Volume.BoundsExtent);
	PassParameters->DensityTexture = DensityTexture;
	PassParameters->VelocityIn = VelocityTexture;
	PassParameters->WarpTexture = WarpTexture;
	PassParameters->BulletCutoutTexture = BulletCutoutTexture;
	PassParameters->BulletSinkTexture = BulletSinkTexture;
	PassParameters->EventCount = ActorEventCount;
	PassParameters->LocalToWorld = State.Volume.LocalToWorld.ToMatrixWithScale();
	PassParameters->Events = GraphBuilder.CreateSRV(ActorEventBuffer);
	PassParameters->ActorAirflowMinSpeed = FMath::Max(0.0f, State.Volume.Settings.ActorAirflowMinSpeed);
	PassParameters->ActorAirflowFullSpeed = FMath::Max(PassParameters->ActorAirflowMinSpeed + 1.0f, State.Volume.Settings.ActorAirflowFullSpeed);
	PassParameters->ActorAirflowRadiusScale = FMath::Max(0.1f, State.Volume.Settings.ActorAirflowRadiusScale);
	PassParameters->SparseVelocityActiveThreshold = FMath::Max(0.0f, State.Volume.Settings.SparseVelocityActiveThreshold);
	PassParameters->OutBrickOccupancy = GraphBuilder.CreateUAV(BrickActivityTexture);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.BuildBrickOccupancy SmokeId=%d", State.Volume.SmokeId),
		ComputeShader,
		PassParameters,
		GroupCount);
}

FTimeThiefSmokeViewExtension::FActiveBrickDispatchResources FTimeThiefSmokeViewExtension::AddExpandBrickOccupancyPass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGTextureRef BrickActivityTexture,
	FRDGTextureRef BrickOccupancyTexture,
	bool bQueueActiveBrickCountReadback)
{
	FActiveBrickDispatchResources Resources;
	const FIntVector BrickGridSize = State.AllocatedBrickGridSize;
	const FIntVector GroupCount = MakeGroupCount(BrickGridSize);
	const int32 MaxActiveSmokeBricks = FMath::Max(State.Volume.Settings.MaxActiveSmokeBricks, 1);

	TShaderMapRef<FTimeThiefSmokeExpandBrickOccupancyCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FRDGBufferRef BrickAllocatorBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1),
		TEXT("TimeThiefSmoke.BrickAllocator"));
	FRDGBufferUAVRef BrickAllocatorUAV = GraphBuilder.CreateUAV(BrickAllocatorBuffer);
	AddClearUAVPass(GraphBuilder, BrickAllocatorUAV, 0u);
	FRDGBufferRef ActiveBricksBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32) * 4, MaxActiveSmokeBricks),
		TEXT("TimeThiefSmoke.ActiveBricks"));

	FTimeThiefSmokeExpandBrickOccupancyCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeExpandBrickOccupancyCS::FParameters>();
	PassParameters->BrickGridResolution = BrickGridSize;
	PassParameters->MaxActiveSmokeBricks = MaxActiveSmokeBricks;
	PassParameters->BrickActivityTexture = BrickActivityTexture;
	PassParameters->BrickAllocator = BrickAllocatorUAV;
	PassParameters->OutActiveBricks = GraphBuilder.CreateUAV(ActiveBricksBuffer);
	PassParameters->OutBrickOccupancy = GraphBuilder.CreateUAV(BrickOccupancyTexture);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.ExpandBrickOccupancy SmokeId=%d", State.Volume.SmokeId),
		ComputeShader,
		PassParameters,
		GroupCount);

	if (bQueueActiveBrickCountReadback && !State.ActiveBrickCountReadback.IsValid())
	{
		State.ActiveBrickCountReadback = MakeShared<FRHIGPUBufferReadback>(TEXT("TimeThiefSmoke.ActiveBrickCountReadback"));
	}
	if (bQueueActiveBrickCountReadback && !State.bActiveBrickCountReadbackPending)
	{
		AddEnqueueCopyPass(GraphBuilder, State.ActiveBrickCountReadback.Get(), BrickAllocatorBuffer, sizeof(uint32));
		State.bActiveBrickCountReadbackPending = true;
	}

	Resources.ActiveBrickCountBuffer = BrickAllocatorBuffer;
	Resources.ActiveBricksBuffer = ActiveBricksBuffer;
	return Resources;
}

void FTimeThiefSmokeViewExtension::AddScatterSparseAtlasPass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGTextureRef DensityTexture,
	FRDGTextureRef WarpTexture,
	FRDGTextureRef BulletCutoutTexture,
	FRDGTextureRef BulletSinkTexture,
	const FActiveBrickDispatchResources& ActiveBrickResources)
{
	const int32 BrickSize = FMath::Clamp(State.Volume.Settings.SmokeBrickSize, 4, 32);
	const FIntVector GridSize = State.AllocatedGridSize;
	if (!ActiveBrickResources.ActiveBrickCountBuffer ||
		!ActiveBrickResources.ActiveBricksBuffer ||
		!State.SparseFieldAtlasTexture.IsValid() ||
		FMath::Min3(State.AllocatedSparseAtlasBrickGridSize.X, State.AllocatedSparseAtlasBrickGridSize.Y, State.AllocatedSparseAtlasBrickGridSize.Z) <= 0 ||
		FMath::Min3(State.AllocatedSparseAtlasGridSize.X, State.AllocatedSparseAtlasGridSize.Y, State.AllocatedSparseAtlasGridSize.Z) <= 0)
	{
		return;
	}
	const int32 SparseScatterGroupsPerBrickAxis = FMath::DivideAndRoundUp(BrickSize, static_cast<int32>(SmokeThreadGroupSize));
	const int32 SparseScatterGroupsPerBrick = FMath::Max(1, SparseScatterGroupsPerBrickAxis * SparseScatterGroupsPerBrickAxis * SparseScatterGroupsPerBrickAxis);
	const int32 SparseDispatchBrickSlots = FMath::Clamp(FMath::Max(State.Volume.Settings.MaxActiveSmokeBricks, 1), 1, 65535);
	const FIntVector GroupCount(SparseScatterGroupsPerBrick, SparseDispatchBrickSlots, 1);

	TShaderMapRef<FTimeThiefSmokeScatterSparseAtlasCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokeScatterSparseAtlasCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeScatterSparseAtlasCS::FParameters>();
	PassParameters->GridResolution = GridSize;
	PassParameters->BrickGridResolution = State.AllocatedBrickGridSize;
	PassParameters->SparseAtlasBrickGridResolution = State.AllocatedSparseAtlasBrickGridSize;
	PassParameters->SmokeBrickSize = BrickSize;
	PassParameters->MaxActiveSmokeBricks = FMath::Max(State.Volume.Settings.MaxActiveSmokeBricks, 1);
	PassParameters->ActiveBrickCountBuffer = GraphBuilder.CreateSRV(ActiveBrickResources.ActiveBrickCountBuffer);
	PassParameters->ActiveBricks = GraphBuilder.CreateSRV(ActiveBrickResources.ActiveBricksBuffer);
	PassParameters->DensityTexture = DensityTexture;
	PassParameters->WarpTexture = WarpTexture;
	PassParameters->BulletCutoutTexture = BulletCutoutTexture;
	PassParameters->BulletSinkTexture = BulletSinkTexture;
	FRDGTextureRef SparseFieldAtlasTexture = GraphBuilder.RegisterExternalTexture(State.SparseFieldAtlasTexture);
	if (!SparseFieldAtlasTexture)
	{
		return;
	}
	PassParameters->OutSparseFieldAtlas = GraphBuilder.CreateUAV(SparseFieldAtlasTexture);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.ScatterSparseAtlas SmokeId=%d", State.Volume.SmokeId),
		ComputeShader,
		PassParameters,
		GroupCount);
}

void FTimeThiefSmokeViewExtension::AddBuildMacVelocityPass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGTextureRef VelocityIn,
	FRDGTextureRef FaceVelocityUOut,
	FRDGTextureRef FaceVelocityVOut,
	FRDGTextureRef FaceVelocityWOut)
{
	const FIntVector GridSize = State.AllocatedGridSize;
	const FIntVector GroupCount = MakeGroupCount(GridSize);
	const FVector3f CellSize = MakeCellSize(State.Volume, GridSize);

	TShaderMapRef<FTimeThiefSmokeBuildMacVelocityCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokeBuildMacVelocityCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeBuildMacVelocityCS::FParameters>();
	PassParameters->GridResolution = GridSize;
	PassParameters->CellSize = CellSize;
	PassParameters->VelocityIn = VelocityIn;
	PassParameters->ObstacleTexture = GraphBuilder.RegisterExternalTexture(State.ObstacleTexture);
	PassParameters->VolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->OutFaceVelocityU = GraphBuilder.CreateUAV(FaceVelocityUOut);
	PassParameters->OutFaceVelocityV = GraphBuilder.CreateUAV(FaceVelocityVOut);
	PassParameters->OutFaceVelocityW = GraphBuilder.CreateUAV(FaceVelocityWOut);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.BuildMacVelocity SmokeId=%d", State.Volume.SmokeId),
		ComputeShader,
		PassParameters,
		GroupCount);
}

void FTimeThiefSmokeViewExtension::AddMacDivergencePass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGTextureRef FaceVelocityUIn,
	FRDGTextureRef FaceVelocityVIn,
	FRDGTextureRef FaceVelocityWIn,
	FRDGTextureRef DivergenceOut,
	FRDGTextureRef PressureOut)
{
	const FIntVector GridSize = State.AllocatedGridSize;
	const FIntVector GroupCount = MakeGroupCount(GridSize);
	const FVector3f CellSize = MakeCellSize(State.Volume, GridSize);

	TShaderMapRef<FTimeThiefSmokeMacDivergenceCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokeMacDivergenceCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeMacDivergenceCS::FParameters>();
	PassParameters->GridResolution = GridSize;
	PassParameters->CellSize = CellSize;
	PassParameters->FaceVelocityUIn = FaceVelocityUIn;
	PassParameters->FaceVelocityVIn = FaceVelocityVIn;
	PassParameters->FaceVelocityWIn = FaceVelocityWIn;
	PassParameters->ObstacleTexture = GraphBuilder.RegisterExternalTexture(State.ObstacleTexture);
	PassParameters->VolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->OutDivergence = GraphBuilder.CreateUAV(DivergenceOut);
	PassParameters->OutPressure = GraphBuilder.CreateUAV(PressureOut);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.MacDivergence SmokeId=%d", State.Volume.SmokeId),
		ComputeShader,
		PassParameters,
		GroupCount);
}

FRDGTextureRef FTimeThiefSmokeViewExtension::AddPressureSolvePass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGTextureRef DivergenceIn,
	FRDGTextureRef ObstacleTexture,
	FRDGTextureRef PressureA,
	FRDGTextureRef PressureB)
{
	FIntVector LevelGridSizes[MultigridMaxLevelCount] = {};
	FVector3f LevelCellSizes[MultigridMaxLevelCount] = {};
	FRDGTextureRef LevelDivergences[MultigridMaxLevelCount] = {};
	FRDGTextureRef LevelPressures[MultigridMaxLevelCount][2] = {};
	FRDGTextureRef LevelResiduals[MultigridMaxLevelCount - 1] = {};

	LevelGridSizes[0] = State.AllocatedGridSize;
	LevelCellSizes[0] = MakeCellSize(State.Volume, LevelGridSizes[0]);
	LevelDivergences[0] = DivergenceIn;
	LevelPressures[0][0] = PressureA;
	LevelPressures[0][1] = PressureB;

	int32 LevelCount = 1;
	while (LevelCount < MultigridMaxLevelCount)
	{
		const FIntVector NextGridSize = MakeCoarseGridSize(LevelGridSizes[LevelCount - 1]);
		if (NextGridSize == LevelGridSizes[LevelCount - 1])
		{
			break;
		}

		LevelGridSizes[LevelCount] = NextGridSize;
		LevelCellSizes[LevelCount] = MakeCellSize(State.Volume, NextGridSize);
		LevelDivergences[LevelCount] = CreateTransientScalarTexture(GraphBuilder, NextGridSize, TEXT("TimeThiefSmoke.MultigridDivergence"));
		LevelPressures[LevelCount][0] = CreateTransientScalarTexture(GraphBuilder, NextGridSize, TEXT("TimeThiefSmoke.MultigridPressureA"));
		LevelPressures[LevelCount][1] = CreateTransientScalarTexture(GraphBuilder, NextGridSize, TEXT("TimeThiefSmoke.MultigridPressureB"));
		++LevelCount;
	}

	for (int32 Level = 0; Level < LevelCount - 1; ++Level)
	{
		LevelResiduals[Level] = CreateTransientScalarTexture(GraphBuilder, LevelGridSizes[Level], TEXT("TimeThiefSmoke.MultigridResidual"));
	}

	FRDGTextureRef CurrentPressure[MultigridMaxLevelCount] = {};
	FRDGTextureRef AlternatePressure[MultigridMaxLevelCount] = {};
	CurrentPressure[0] = LevelPressures[0][0];
	AlternatePressure[0] = LevelPressures[0][1];

	if (LevelCount == 1)
	{
		for (int32 Iteration = 0; Iteration < MultigridCoarsestSmoothPassCount; ++Iteration)
		{
			AddPressureJacobiPass(GraphBuilder, State, LevelGridSizes[0], LevelCellSizes[0], CurrentPressure[0], LevelDivergences[0], ObstacleTexture, AlternatePressure[0]);
			Swap(CurrentPressure[0], AlternatePressure[0]);
		}
		return CurrentPressure[0];
	}

	for (int32 Cycle = 0; Cycle < MultigridCycleCount; ++Cycle)
	{
		for (int32 Level = 0; Level < LevelCount - 1; ++Level)
		{
			for (int32 Iteration = 0; Iteration < MultigridPreSmoothPassCount; ++Iteration)
			{
				AddPressureJacobiPass(GraphBuilder, State, LevelGridSizes[Level], LevelCellSizes[Level], CurrentPressure[Level], LevelDivergences[Level], ObstacleTexture, AlternatePressure[Level]);
				Swap(CurrentPressure[Level], AlternatePressure[Level]);
			}

			AddPressureResidualPass(GraphBuilder, State, LevelGridSizes[Level], LevelCellSizes[Level], CurrentPressure[Level], LevelDivergences[Level], ObstacleTexture, LevelResiduals[Level]);
			AddPressureRestrictPass(GraphBuilder, LevelGridSizes[Level], LevelGridSizes[Level + 1], LevelResiduals[Level], LevelDivergences[Level + 1]);

			CurrentPressure[Level + 1] = LevelPressures[Level + 1][0];
			AlternatePressure[Level + 1] = LevelPressures[Level + 1][1];
			AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(CurrentPressure[Level + 1]), 0.0f);
			AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(AlternatePressure[Level + 1]), 0.0f);
		}

		const int32 CoarsestLevel = LevelCount - 1;
		for (int32 Iteration = 0; Iteration < MultigridCoarsestSmoothPassCount; ++Iteration)
		{
			AddPressureJacobiPass(GraphBuilder, State, LevelGridSizes[CoarsestLevel], LevelCellSizes[CoarsestLevel], CurrentPressure[CoarsestLevel], LevelDivergences[CoarsestLevel], ObstacleTexture, AlternatePressure[CoarsestLevel]);
			Swap(CurrentPressure[CoarsestLevel], AlternatePressure[CoarsestLevel]);
		}

		for (int32 Level = CoarsestLevel - 1; Level >= 0; --Level)
		{
			AddPressureProlongateAddPass(GraphBuilder, LevelGridSizes[Level], LevelGridSizes[Level + 1], CurrentPressure[Level], CurrentPressure[Level + 1], AlternatePressure[Level]);
			Swap(CurrentPressure[Level], AlternatePressure[Level]);

			for (int32 Iteration = 0; Iteration < MultigridPostSmoothPassCount; ++Iteration)
			{
				AddPressureJacobiPass(GraphBuilder, State, LevelGridSizes[Level], LevelCellSizes[Level], CurrentPressure[Level], LevelDivergences[Level], ObstacleTexture, AlternatePressure[Level]);
				Swap(CurrentPressure[Level], AlternatePressure[Level]);
			}
		}
	}

	return CurrentPressure[0];
}

void FTimeThiefSmokeViewExtension::AddPressureJacobiPass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	const FIntVector& GridSize,
	const FVector3f& CellSize,
	FRDGTextureRef PressureIn,
	FRDGTextureRef DivergenceIn,
	FRDGTextureRef ObstacleTexture,
	FRDGTextureRef PressureOut)
{
	const FIntVector GroupCount = MakeGroupCount(GridSize);

	TShaderMapRef<FTimeThiefSmokePressureJacobiCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokePressureJacobiCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokePressureJacobiCS::FParameters>();
	PassParameters->GridResolution = GridSize;
	PassParameters->CellSize = CellSize;
	PassParameters->PressureIn = PressureIn;
	PassParameters->DivergenceIn = DivergenceIn;
	PassParameters->ObstacleTexture = ObstacleTexture;
	PassParameters->VolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->OutPressure = GraphBuilder.CreateUAV(PressureOut);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.PressureJacobi SmokeId=%d", State.Volume.SmokeId),
		ComputeShader,
		PassParameters,
		GroupCount);
}

void FTimeThiefSmokeViewExtension::AddPressureResidualPass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	const FIntVector& GridSize,
	const FVector3f& CellSize,
	FRDGTextureRef PressureIn,
	FRDGTextureRef DivergenceIn,
	FRDGTextureRef ObstacleTexture,
	FRDGTextureRef ResidualOut)
{
	const FIntVector GroupCount = MakeGroupCount(GridSize);

	TShaderMapRef<FTimeThiefSmokePressureResidualCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokePressureResidualCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokePressureResidualCS::FParameters>();
	PassParameters->GridResolution = GridSize;
	PassParameters->CellSize = CellSize;
	PassParameters->PressureIn = PressureIn;
	PassParameters->DivergenceIn = DivergenceIn;
	PassParameters->ObstacleTexture = ObstacleTexture;
	PassParameters->VolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->OutDivergence = GraphBuilder.CreateUAV(ResidualOut);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.PressureResidual SmokeId=%d", State.Volume.SmokeId),
		ComputeShader,
		PassParameters,
		GroupCount);
}

void FTimeThiefSmokeViewExtension::AddPressureRestrictPass(
	FRDGBuilder& GraphBuilder,
	const FIntVector& FineGridSize,
	const FIntVector& CoarseGridSize,
	FRDGTextureRef FineDivergenceIn,
	FRDGTextureRef CoarseDivergenceOut)
{
	const FIntVector GroupCount = MakeGroupCount(CoarseGridSize);

	TShaderMapRef<FTimeThiefSmokePressureRestrictCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokePressureRestrictCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokePressureRestrictCS::FParameters>();
	PassParameters->GridResolution = CoarseGridSize;
	PassParameters->FineGridResolution = FineGridSize;
	PassParameters->DivergenceIn = FineDivergenceIn;
	PassParameters->OutDivergence = GraphBuilder.CreateUAV(CoarseDivergenceOut);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.PressureRestrict"),
		ComputeShader,
		PassParameters,
		GroupCount);
}

void FTimeThiefSmokeViewExtension::AddPressureProlongateAddPass(
	FRDGBuilder& GraphBuilder,
	const FIntVector& FineGridSize,
	const FIntVector& CoarseGridSize,
	FRDGTextureRef FinePressureIn,
	FRDGTextureRef CoarsePressureIn,
	FRDGTextureRef FinePressureOut)
{
	const FIntVector GroupCount = MakeGroupCount(FineGridSize);

	TShaderMapRef<FTimeThiefSmokePressureProlongateAddCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokePressureProlongateAddCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokePressureProlongateAddCS::FParameters>();
	PassParameters->GridResolution = FineGridSize;
	PassParameters->CoarseGridResolution = CoarseGridSize;
	PassParameters->PressureIn = FinePressureIn;
	PassParameters->CoarsePressureIn = CoarsePressureIn;
	PassParameters->VolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->OutPressure = GraphBuilder.CreateUAV(FinePressureOut);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.PressureProlongateAdd"),
		ComputeShader,
		PassParameters,
		GroupCount);
}

void FTimeThiefSmokeViewExtension::AddProjectVelocityPass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGTextureRef VelocityIn,
	FRDGTextureRef PressureIn,
	FRDGTextureRef VelocityOut)
{
	const FIntVector GridSize = State.AllocatedGridSize;
	const FIntVector GroupCount = MakeGroupCount(GridSize);
	const FVector3f CellSize = MakeCellSize(State.Volume, GridSize);

	TShaderMapRef<FTimeThiefSmokeProjectVelocityCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokeProjectVelocityCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeProjectVelocityCS::FParameters>();
	PassParameters->GridResolution = GridSize;
	PassParameters->CellSize = CellSize;
	PassParameters->VelocityIn = VelocityIn;
	PassParameters->PressureIn = PressureIn;
	PassParameters->ObstacleTexture = GraphBuilder.RegisterExternalTexture(State.ObstacleTexture);
	PassParameters->VolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->OutVelocity = GraphBuilder.CreateUAV(VelocityOut);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.ProjectVelocity SmokeId=%d", State.Volume.SmokeId),
		ComputeShader,
		PassParameters,
		GroupCount);
}

void FTimeThiefSmokeViewExtension::AddProjectMacVelocityPass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGTextureRef FaceVelocityUIn,
	FRDGTextureRef FaceVelocityVIn,
	FRDGTextureRef FaceVelocityWIn,
	FRDGTextureRef PressureIn,
	FRDGTextureRef FaceVelocityUOut,
	FRDGTextureRef FaceVelocityVOut,
	FRDGTextureRef FaceVelocityWOut)
{
	const FIntVector GridSize = State.AllocatedGridSize;
	const FIntVector GroupCount = MakeGroupCount(GridSize);
	const FVector3f CellSize = MakeCellSize(State.Volume, GridSize);

	TShaderMapRef<FTimeThiefSmokeProjectMacVelocityCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokeProjectMacVelocityCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeProjectMacVelocityCS::FParameters>();
	PassParameters->GridResolution = GridSize;
	PassParameters->CellSize = CellSize;
	PassParameters->FaceVelocityUIn = FaceVelocityUIn;
	PassParameters->FaceVelocityVIn = FaceVelocityVIn;
	PassParameters->FaceVelocityWIn = FaceVelocityWIn;
	PassParameters->PressureIn = PressureIn;
	PassParameters->ObstacleTexture = GraphBuilder.RegisterExternalTexture(State.ObstacleTexture);
	PassParameters->VolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->OutFaceVelocityU = GraphBuilder.CreateUAV(FaceVelocityUOut);
	PassParameters->OutFaceVelocityV = GraphBuilder.CreateUAV(FaceVelocityVOut);
	PassParameters->OutFaceVelocityW = GraphBuilder.CreateUAV(FaceVelocityWOut);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.ProjectMacVelocity SmokeId=%d", State.Volume.SmokeId),
		ComputeShader,
		PassParameters,
		GroupCount);
}

void FTimeThiefSmokeViewExtension::AddMacToCollocatedVelocityPass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGTextureRef FaceVelocityUIn,
	FRDGTextureRef FaceVelocityVIn,
	FRDGTextureRef FaceVelocityWIn,
	FRDGTextureRef VelocityOut)
{
	const FIntVector GridSize = State.AllocatedGridSize;
	const FIntVector GroupCount = MakeGroupCount(GridSize);
	const FVector3f CellSize = MakeCellSize(State.Volume, GridSize);

	TShaderMapRef<FTimeThiefSmokeMacToCollocatedVelocityCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokeMacToCollocatedVelocityCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeMacToCollocatedVelocityCS::FParameters>();
	PassParameters->GridResolution = GridSize;
	PassParameters->CellSize = CellSize;
	PassParameters->FaceVelocityUIn = FaceVelocityUIn;
	PassParameters->FaceVelocityVIn = FaceVelocityVIn;
	PassParameters->FaceVelocityWIn = FaceVelocityWIn;
	PassParameters->ObstacleTexture = GraphBuilder.RegisterExternalTexture(State.ObstacleTexture);
	PassParameters->VolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->OutVelocity = GraphBuilder.CreateUAV(VelocityOut);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.MacToCollocatedVelocity SmokeId=%d", State.Volume.SmokeId),
		ComputeShader,
		PassParameters,
		GroupCount);
}
