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
	constexpr int32 MaxCarrierParticleCount = 128;
	constexpr int32 MaxVortexParticleCount = 128;
	constexpr int32 MaxShaderEventCount = 128;
	constexpr int32 MaxDebugEventCount = 128;
	constexpr float ObstacleMaskBlendDuration = 0.25f;
	constexpr int32 MultigridMaxLevelCount = 4;
	constexpr int32 MultigridCycleCount = 2;
	constexpr int32 MultigridPreSmoothPassCount = 2;
	constexpr int32 MultigridPostSmoothPassCount = 2;
	constexpr int32 MultigridCoarsestSmoothPassCount = 12;

	static TAutoConsoleVariable<int32> CVarTimeThiefSmokeDebugView(
		TEXT("r.TimeThiefSmoke.DebugView"),
		0,
		TEXT("Custom smoke debug view. 0=off, 1=density, 2=obstacle, 3=bullet fields, 4=carrier particles, 5=events, 6=warp, 7=active bricks, 8=divergence, 9=bounds, 10=analytic bullet."));

	static TAutoConsoleVariable<int32> CVarTimeThiefSmokeScissor(
		TEXT("r.TimeThiefSmoke.Scissor"),
		1,
		TEXT("Limits custom smoke composite draw calls to each smoke AABB screen rect when enabled."));

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

	int32 NextLowerSmokeResolutionTier(const int32 Resolution)
	{
		if (Resolution > 384)
		{
			return 384;
		}
		return 256;
	}

	FIntVector MakeBudgetedGridSize(
		const FTimeThiefSmokeRendererVolume& Volume,
		const int32 SparseResolutionCap,
		int32& OutEffectiveResolution,
		bool& bOutBudgetOverflow)
	{
		const int32 RequestedResolution = FMath::Clamp(Volume.Settings.SmokeGridResolution, 16, 512);
		const int32 ResolutionCap = SparseResolutionCap > 0 ? FMath::Clamp(SparseResolutionCap, 16, 512) : RequestedResolution;
		int32 EffectiveResolution = FMath::Min(RequestedResolution, ResolutionCap);
		bOutBudgetOverflow = EffectiveResolution < RequestedResolution;
		OutEffectiveResolution = EffectiveResolution;
		return MakeGridSize(EffectiveResolution, Volume.BoundsExtent);
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

	FTransform ToDoubleTransform(const FTransform3f& Transform)
	{
		return FTransform(
			FQuat(Transform.GetRotation()),
			FVector(Transform.GetTranslation()),
			FVector(Transform.GetScale3D()));
	}

	bool HasEventType(const TArray<FTimeThiefSmokeRendererEvent>& Events, ETimeThiefSmokeRendererInteractionType Type)
	{
		for (const FTimeThiefSmokeRendererEvent& Event : Events)
		{
			if (Event.Type == Type)
			{
				return true;
			}
		}
		return false;
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
			State.SparseResolutionCap = 0;
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
				const int32 CurrentResolution = State.EffectiveSmokeGridResolution > 0
					? State.EffectiveSmokeGridResolution
					: FMath::Clamp(Volume.Settings.SmokeGridResolution, 16, 512);
				const int32 NextResolutionCap = NextLowerSmokeResolutionTier(CurrentResolution);
				if (NextResolutionCap >= CurrentResolution)
				{
					const bool bFallbackChanged = !State.bForceDenseComposite;
					State.bForceDenseComposite = true;
					if (!State.bWarnedBrickBudgetOverflow || bFallbackChanged)
					{
						UE_LOG(
							LogTimeThiefSmokeRenderer,
							Warning,
							TEXT("SmokeId %d active brick count %u exceeded sparse brick budget %d at minimum sparse resolution %d; using dense composite fallback."),
							Volume.SmokeId,
							State.LastActiveBrickCount,
							MaxActiveSmokeBricks,
							CurrentResolution);
					}
				}
				else
				{
					const bool bCapChanged = State.SparseResolutionCap <= 0 || NextResolutionCap < State.SparseResolutionCap;
					State.SparseResolutionCap = State.SparseResolutionCap > 0
						? FMath::Min(State.SparseResolutionCap, NextResolutionCap)
						: NextResolutionCap;
					State.bForceDenseComposite = false;
					if (!State.bWarnedBrickBudgetOverflow || bCapChanged)
					{
						UE_LOG(
							LogTimeThiefSmokeRenderer,
							Warning,
							TEXT("SmokeId %d active brick count %u exceeded sparse brick budget %d; capping next-frame resolution to %d."),
							Volume.SmokeId,
							State.LastActiveBrickCount,
							MaxActiveSmokeBricks,
							State.SparseResolutionCap);
					}
				}
				State.bWarnedBrickBudgetOverflow = true;
			}
			else
			{
				State.bForceDenseComposite = false;
				if (State.SparseResolutionCap > 0 && State.LastActiveBrickCount < static_cast<uint32>(MaxActiveSmokeBricks / 2))
				{
					State.SparseResolutionCap = 0;
					State.bWarnedBrickBudgetOverflow = false;
				}
			}
		}
		if (bSparseBackend && State.SparseResolutionCap > 0 && Volume.Settings.SmokeGridResolution <= State.SparseResolutionCap)
		{
			State.SparseResolutionCap = 0;
			State.bWarnedBrickBudgetOverflow = false;
		}
		int32 EffectiveResolution = 0;
		bool bBrickBudgetOverflow = false;
		const FIntVector NewGridSize = MakeBudgetedGridSize(Volume, bSparseBackend ? State.SparseResolutionCap : 0, EffectiveResolution, bBrickBudgetOverflow);
		if (!bSparseBackend || (!bBrickBudgetOverflow && !State.bForceDenseComposite))
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
			State.BulletImpulseTexture.SafeRelease();
			State.WarpTextures[0].SafeRelease();
			State.WarpTextures[1].SafeRelease();
			State.ObstacleTexture.SafeRelease();
			State.BrickOccupancyTexture.SafeRelease();
			State.SparseDensityAtlasTexture.SafeRelease();
			State.SparseWarpAtlasTexture.SafeRelease();
			State.SparseBulletCutoutAtlasTexture.SafeRelease();
			State.SparseBulletSinkAtlasTexture.SafeRelease();
			State.CarrierParticleBuffers[0].SafeRelease();
			State.CarrierParticleBuffers[1].SafeRelease();
			State.VortexParticleBuffers[0].SafeRelease();
			State.VortexParticleBuffers[1].SafeRelease();
			State.CurrentDensityIndex = 0;
			State.CurrentVelocityIndex = 0;
			State.CurrentBulletFieldIndex = 0;
			State.CurrentWarpIndex = 0;
			State.CurrentCarrierParticleIndex = 0;
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
			State.AllocatedCarrierParticleCount = 0;
			State.AllocatedVortexParticleCount = 0;
			State.bCarrierParticlesNeedUpload = true;
			State.bVortexParticlesNeedUpload = true;
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
			if (State->PendingEvents.Num() < MaxEvents)
			{
				State->PendingEvents.Add(Event);
				if (State->LastDebugEvents.Num() < MaxDebugEventCount)
				{
					State->LastDebugEvents.Add(Event);
				}
			}

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

		SimulateSmoke(GraphBuilder, State, LastFrameDeltaSeconds);
		State.LastSimulatedFrame = GFrameNumberRenderThread;
	}
}

void FTimeThiefSmokeViewExtension::SubscribeToPostProcessingPass(
	EPostProcessingPass Pass,
	const FSceneView& View,
	FAfterPassCallbackDelegateArray& InOutPassCallbacks,
	bool bIsPassEnabled)
{
	if (Pass == EPostProcessingPass::BeforeDOF)
	{
		InOutPassCallbacks.Add(FAfterPassCallbackDelegate::CreateRaw(this, &FTimeThiefSmokeViewExtension::CompositeSmoke_RenderThread));
	}
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

	const FScreenPassViewInfo ViewInfo(View);
	const FScreenPassTextureViewport InputViewport(CurrentSceneColor);
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
			State.SparseDensityAtlasTexture.IsValid() &&
			State.SparseWarpAtlasTexture.IsValid() &&
			State.SparseBulletCutoutAtlasTexture.IsValid() &&
			State.SparseBulletSinkAtlasTexture.IsValid() &&
			State.CarrierParticleBuffers[State.CurrentCarrierParticleIndex].IsValid())
		{
			RenderStates.Add(&State);
		}
	}

	if (RenderStates.IsEmpty())
	{
		return Inputs.ReturnUntouchedSceneColorForPostProcessing(GraphBuilder);
	}

	for (int32 StateIndex = 0; StateIndex < RenderStates.Num(); ++StateIndex)
	{
		FRenderSmokeState& State = *RenderStates[StateIndex];
		const bool bIsLastSmoke = StateIndex == RenderStates.Num() - 1;
		FIntRect SmokeRect = CurrentSceneColor.ViewRect;
		if (bUseScissor && !ComputeSmokeScreenRect(View, State.Volume, CurrentSceneColor.ViewRect, SmokeRect))
		{
			continue;
		}

		FScreenPassRenderTarget Output = bIsLastSmoke && Inputs.OverrideOutput.IsValid()
			? Inputs.OverrideOutput
			: FScreenPassRenderTarget::CreateFromInput(GraphBuilder, CurrentSceneColor, ERenderTargetLoadAction::ELoad, TEXT("TimeThiefSmoke.Composite"));

		if (Output.Texture != CurrentSceneColor.Texture)
		{
			AddCopyTexturePass(GraphBuilder, CurrentSceneColor.Texture, Output.Texture);
		}

		TArray<FTimeThiefSmokeEventShaderData> ShaderEvents;
		ShaderEvents.Reserve(State.LastDebugEvents.Num() + State.AnalyticBulletEvents.Num());
		for (const FTimeThiefSmokeRendererEvent& Event : State.AnalyticBulletEvents)
		{
			if (ShaderEvents.Num() >= MaxDebugEventCount)
			{
				break;
			}
			ShaderEvents.Add(ToShaderEvent(Event));
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
		FRDGBufferRef CarrierBuffer = GraphBuilder.RegisterExternalBuffer(
			State.CarrierParticleBuffers[State.CurrentCarrierParticleIndex],
			TEXT("TimeThiefSmoke.CompositeCarrierParticles"));

		TShaderMapRef<FTimeThiefSmokeCompositePS> PixelShader(GetGlobalShaderMap(View.FeatureLevel));
		FTimeThiefSmokeCompositePS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeCompositePS::FParameters>();
		PassParameters->SceneColorTexture = CurrentSceneColor.Texture;
		PassParameters->SceneDepthTexture = Inputs.SceneTextures.SceneTextures->GetParameters()->SceneDepthTexture;
		PassParameters->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		PassParameters->DensityTexture = GraphBuilder.RegisterExternalTexture(State.DensityTextures[State.CurrentDensityIndex]);
		PassParameters->DivergenceTexture = GraphBuilder.RegisterExternalTexture(State.DivergenceTexture);
		PassParameters->BrickOccupancyTexture = GraphBuilder.RegisterExternalTexture(State.BrickOccupancyTexture);
		PassParameters->SparseDensityAtlasTexture = GraphBuilder.RegisterExternalTexture(State.SparseDensityAtlasTexture);
		PassParameters->SparseWarpAtlasTexture = GraphBuilder.RegisterExternalTexture(State.SparseWarpAtlasTexture);
		PassParameters->SparseBulletCutoutAtlasTexture = GraphBuilder.RegisterExternalTexture(State.SparseBulletCutoutAtlasTexture);
		PassParameters->SparseBulletSinkAtlasTexture = GraphBuilder.RegisterExternalTexture(State.SparseBulletSinkAtlasTexture);
		PassParameters->WarpTexture = GraphBuilder.RegisterExternalTexture(State.WarpTextures[State.CurrentWarpIndex]);
		PassParameters->ObstacleTexture = GraphBuilder.RegisterExternalTexture(State.ObstacleTexture);
		PassParameters->BulletCutoutTexture = GraphBuilder.RegisterExternalTexture(State.BulletCutoutTextures[State.CurrentBulletFieldIndex]);
		PassParameters->BulletSinkTexture = GraphBuilder.RegisterExternalTexture(State.BulletSinkTextures[State.CurrentBulletFieldIndex]);
		PassParameters->VolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		PassParameters->CarrierParticles = GraphBuilder.CreateSRV(CarrierBuffer);
		PassParameters->Events = GraphBuilder.CreateSRV(EventBuffer);
		PassParameters->SceneColorUVScaleBias = FVector4f(
			1.0f / FMath::Max(1, CurrentSceneColor.Texture->Desc.Extent.X),
			1.0f / FMath::Max(1, CurrentSceneColor.Texture->Desc.Extent.Y),
			0.0f,
			0.0f);
		PassParameters->ViewRect = CurrentSceneColor.ViewRect;
		PassParameters->GridResolution = State.AllocatedGridSize;
		PassParameters->bUseSparseAtlas = (State.Volume.Settings.SimulationBackend == ETimeThiefSmokeSimulationBackend::SparseMac && !State.bForceDenseComposite) ? 1u : 0u;
		PassParameters->MaxActiveSmokeBricks = FMath::Max(State.Volume.Settings.MaxActiveSmokeBricks, 1);
		PassParameters->BrickGridResolution = State.AllocatedBrickGridSize;
		PassParameters->SparseAtlasBrickGridResolution = State.AllocatedSparseAtlasBrickGridSize;
		PassParameters->SmokeBrickSize = FMath::Clamp(State.Volume.Settings.SmokeBrickSize, 4, 32);
		PassParameters->BoundsExtent = FVector3f(State.Volume.BoundsExtent);
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
		PassParameters->RenderStepCount = FMath::Clamp(State.Volume.Settings.RenderStepCount, 16, 512);
		PassParameters->RenderMaxStepCount = FMath::Clamp(State.Volume.Settings.RenderMaxStepCount, 16, 1024);
		PassParameters->RenderStepVoxelScale = FMath::Clamp(State.Volume.Settings.RenderStepVoxelScale, 0.1f, 4.0f);
		PassParameters->DebugMode = DebugMode;
		PassParameters->CarrierParticleCount = FMath::Min(State.AllocatedCarrierParticleCount, MaxCarrierParticleCount);
		PassParameters->EventCount = ShaderEventCount;
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

	return CurrentSceneColor;
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
	const FRDGTextureDesc BrickOccupancyDesc = FRDGTextureDesc::Create3D(
		BrickGridSize,
		PF_R32_UINT,
		FClearValueBinding::Black,
		TexCreate_ShaderResource | TexCreate_UAV);
	const FRDGTextureDesc SparseAtlasDesc = FRDGTextureDesc::Create3D(
		SparseAtlasGridSize,
		PF_R16F,
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

	if (!State.BulletImpulseTexture.IsValid())
	{
		AllocatePooledTexture(VelocityDesc, State.BulletImpulseTexture, TEXT("TimeThiefSmoke.BulletImpulse"));
		State.bNeedsInit = true;
	}

	if (!State.BrickOccupancyTexture.IsValid() || State.AllocatedBrickGridSize != BrickGridSize)
	{
		State.BrickOccupancyTexture.SafeRelease();
		AllocatePooledTexture(BrickOccupancyDesc, State.BrickOccupancyTexture, TEXT("TimeThiefSmoke.BrickOccupancy"));
		State.AllocatedBrickGridSize = BrickGridSize;
	}

	if (!State.SparseDensityAtlasTexture.IsValid() ||
		!State.SparseWarpAtlasTexture.IsValid() ||
		!State.SparseBulletCutoutAtlasTexture.IsValid() ||
		!State.SparseBulletSinkAtlasTexture.IsValid() ||
		State.AllocatedSparseAtlasGridSize != SparseAtlasGridSize)
	{
		State.SparseDensityAtlasTexture.SafeRelease();
		State.SparseWarpAtlasTexture.SafeRelease();
		State.SparseBulletCutoutAtlasTexture.SafeRelease();
		State.SparseBulletSinkAtlasTexture.SafeRelease();
		AllocatePooledTexture(SparseAtlasDesc, State.SparseDensityAtlasTexture, TEXT("TimeThiefSmoke.SparseDensityAtlas"));
		AllocatePooledTexture(SparseAtlasDesc, State.SparseWarpAtlasTexture, TEXT("TimeThiefSmoke.SparseWarpAtlas"));
		AllocatePooledTexture(SparseAtlasDesc, State.SparseBulletCutoutAtlasTexture, TEXT("TimeThiefSmoke.SparseBulletCutoutAtlas"));
		AllocatePooledTexture(SparseAtlasDesc, State.SparseBulletSinkAtlasTexture, TEXT("TimeThiefSmoke.SparseBulletSinkAtlas"));
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

void FTimeThiefSmokeViewExtension::EnsureCarrierParticles(FRenderSmokeState& State)
{
	const int32 DesiredCount = FMath::Clamp(State.Volume.Settings.CarrierParticleCount, 1, MaxCarrierParticleCount);
	if (State.AllocatedCarrierParticleCount == DesiredCount && State.CarrierParticles.Num() == DesiredCount)
	{
		return;
	}

	State.AllocatedCarrierParticleCount = DesiredCount;
	State.CarrierParticleBuffers[0].SafeRelease();
	State.CarrierParticleBuffers[1].SafeRelease();
	State.CurrentCarrierParticleIndex = 0;
	State.CarrierParticles.Reset(DesiredCount);
	State.bCarrierParticlesNeedUpload = true;

	FRandomStream RandomStream(State.Volume.SmokeId * 48271 + 1337);
	const float BaseRadius = FMath::Max(1.0f, State.Volume.Settings.CarrierParticleRadius);
	const float DriftSpeed = FMath::Max(0.0f, State.Volume.Settings.CarrierParticleDriftSpeed);
	const float SourceRadius = FMath::Max(8.0f, State.Volume.Settings.PlumeSourceRadius);

	for (int32 ParticleIndex = 0; ParticleIndex < DesiredCount; ++ParticleIndex)
	{
		const FVector Direction = (RandomStream.VRand() + FVector(0.0, 0.0, 0.22)).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
		const FVector SourceJitter = Direction * SourceRadius * RandomStream.FRandRange(0.03f, 0.28f);

		FRenderSmokeState::FCarrierParticle Particle;
		Particle.LocalPosition = FVector3f(SourceJitter);
		Particle.Radius = BaseRadius * RandomStream.FRandRange(0.72f, 1.28f);
		Particle.Phase = RandomStream.FRandRange(0.0f, UE_TWO_PI);
		const FVector InitialVelocity = Direction * DriftSpeed * RandomStream.FRandRange(0.75f, 1.35f);
		Particle.Velocity = FVector3f(InitialVelocity);
		State.CarrierParticles.Add(Particle);
	}
}

void FTimeThiefSmokeViewExtension::EnsureCarrierParticleBuffers(FRDGBuilder& GraphBuilder, FRenderSmokeState& State)
{
	EnsureCarrierParticles(State);

	const int32 DesiredCount = FMath::Clamp(State.Volume.Settings.CarrierParticleCount, 1, MaxCarrierParticleCount);
	FRDGBufferDesc CarrierBufferDesc = FRDGBufferDesc::CreateStructuredDesc(
		sizeof(FTimeThiefSmokeCarrierParticleShaderData),
		DesiredCount);
	CarrierBufferDesc.Usage |=
		EBufferUsageFlags::ShaderResource |
		EBufferUsageFlags::UnorderedAccess |
		EBufferUsageFlags::SourceCopy;

	for (int32 BufferIndex = 0; BufferIndex < 2; ++BufferIndex)
	{
		if (!State.CarrierParticleBuffers[BufferIndex].IsValid())
		{
			AllocatePooledBuffer(CarrierBufferDesc, State.CarrierParticleBuffers[BufferIndex], TEXT("TimeThiefSmoke.CarrierParticles"));
			State.bCarrierParticlesNeedUpload = true;
		}
	}
}

void FTimeThiefSmokeViewExtension::UploadCarrierParticles(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGBufferRef CarrierBuffer)
{
	TArray<FTimeThiefSmokeCarrierParticleShaderData> ShaderParticles;
	ShaderParticles.Reserve(State.CarrierParticles.Num());
	for (const FRenderSmokeState::FCarrierParticle& Particle : State.CarrierParticles)
	{
		FTimeThiefSmokeCarrierParticleShaderData ShaderParticle;
		ShaderParticle.LocalPositionRadius = FVector4f(
			Particle.LocalPosition.X,
			Particle.LocalPosition.Y,
			Particle.LocalPosition.Z,
			Particle.Radius);
		ShaderParticle.VelocityPhase = FVector4f(
			Particle.Velocity.X,
			Particle.Velocity.Y,
			Particle.Velocity.Z,
			Particle.Phase);
		ShaderParticles.Add(ShaderParticle);
	}
	if (ShaderParticles.IsEmpty())
	{
		ShaderParticles.AddDefaulted();
	}

	FRDGBufferRef UploadBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(FTimeThiefSmokeCarrierParticleShaderData), ShaderParticles.Num()),
		TEXT("TimeThiefSmoke.CarrierUpload"));
	GraphBuilder.QueueBufferUpload(UploadBuffer, ShaderParticles.GetData(), ShaderParticles.Num() * sizeof(FTimeThiefSmokeCarrierParticleShaderData));
	AddCopyBufferPass(GraphBuilder, CarrierBuffer, UploadBuffer);
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

void FTimeThiefSmokeViewExtension::AddCarrierParticleUpdatePass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGBufferRef CarrierIn,
	FRDGBufferRef CarrierOut,
	FRDGBufferRef EventBuffer,
	int32 EventCount,
	float DeltaSeconds)
{
	const int32 CarrierParticleCount = FMath::Clamp(State.Volume.Settings.CarrierParticleCount, 1, MaxCarrierParticleCount);
	TShaderMapRef<FTimeThiefSmokeCarrierUpdateCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokeCarrierUpdateCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeCarrierUpdateCS::FParameters>();
	PassParameters->CarrierParticlesIn = GraphBuilder.CreateSRV(CarrierIn);
	PassParameters->Events = GraphBuilder.CreateSRV(EventBuffer);
	PassParameters->OutCarrierParticles = GraphBuilder.CreateUAV(CarrierOut);
	PassParameters->DeltaSeconds = DeltaSeconds;
	PassParameters->DriftSpeed = State.Volume.Settings.CarrierParticleDriftSpeed;
	PassParameters->InteractionStrength = State.Volume.Settings.CarrierParticleInteractionStrength;
	PassParameters->CarrierParticleCount = CarrierParticleCount;
	PassParameters->EventCount = EventCount;
	PassParameters->BoundsExtent = FVector3f(State.Volume.BoundsExtent);
	PassParameters->NaturalBoundsExtent = FVector3f(State.Volume.NaturalBoundsExtent);
	PassParameters->LocalToWorld = State.Volume.LocalToWorld.ToMatrixWithScale();
	PassParameters->WorldToLocal = State.Volume.LocalToWorld.ToInverseMatrixWithScale();

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.CarrierUpdate SmokeId=%d Events=%d", State.Volume.SmokeId, EventCount),
		ComputeShader,
		PassParameters,
		FIntVector(FMath::DivideAndRoundUp(CarrierParticleCount, 64), 1, 1));
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

void FTimeThiefSmokeViewExtension::AddVortexParticleSplatPass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGBufferRef VortexBuffer,
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
	PassParameters->DensityIn = DensityIn;
	PassParameters->VelocityIn = VelocityIn;
	PassParameters->ObstacleTexture = GraphBuilder.RegisterExternalTexture(State.ObstacleTexture);
	PassParameters->BulletCutoutTexture = BulletCutoutTexture;
	PassParameters->BulletSinkTexture = BulletSinkTexture;
	PassParameters->VolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->OutVelocity = GraphBuilder.CreateUAV(VelocityOut);
	PassParameters->GridResolution = GridSize;
	PassParameters->BoundsExtent = FVector3f(State.Volume.BoundsExtent);
	PassParameters->VortexParticleCount = FMath::Clamp(State.Volume.Settings.VortexParticleCount, 1, MaxVortexParticleCount);
	PassParameters->VortexParticleSplatRadius = FMath::Max(1.0f, State.Volume.Settings.VortexParticleSplatRadius);
	PassParameters->VortexParticleCoreRadius = FMath::Max(1.0f, State.Volume.Settings.VortexParticleCoreRadius);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.SplatVortexParticles SmokeId=%d", State.Volume.SmokeId),
		ComputeShader,
		PassParameters,
		GroupCount);
}

void FTimeThiefSmokeViewExtension::SimulateSmoke(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	float DeltaSeconds)
{
	EnsureResources(GraphBuilder, State);
	EnsureCarrierParticleBuffers(GraphBuilder, State);
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
	FRDGTextureRef BulletImpulseTexture = GraphBuilder.RegisterExternalTexture(State.BulletImpulseTexture);
	FRDGTextureRef WarpTextures[2] =
	{
		GraphBuilder.RegisterExternalTexture(State.WarpTextures[0]),
		GraphBuilder.RegisterExternalTexture(State.WarpTextures[1])
	};
	FRDGTextureRef DivergenceTexture = GraphBuilder.RegisterExternalTexture(State.DivergenceTexture);
	FRDGTextureRef ObstacleTexture = GraphBuilder.RegisterExternalTexture(State.ObstacleTexture);
	FRDGBufferRef CarrierParticleBuffers[2] =
	{
		GraphBuilder.RegisterExternalBuffer(State.CarrierParticleBuffers[0], TEXT("TimeThiefSmoke.CarrierParticles0")),
		GraphBuilder.RegisterExternalBuffer(State.CarrierParticleBuffers[1], TEXT("TimeThiefSmoke.CarrierParticles1"))
	};
	FRDGBufferRef VortexParticleBuffers[2] =
	{
		GraphBuilder.RegisterExternalBuffer(State.VortexParticleBuffers[0], TEXT("TimeThiefSmoke.VortexParticles0")),
		GraphBuilder.RegisterExternalBuffer(State.VortexParticleBuffers[1], TEXT("TimeThiefSmoke.VortexParticles1"))
	};
	int32 SimulationEventCount = 0;
	FRDGBufferRef SimulationEventBuffer = CreateSmokeEventBuffer(
		GraphBuilder,
		State.PendingEvents,
		SimulationEventCount,
		TEXT("TimeThiefSmoke.SimulationEvents"));

	if (State.bCarrierParticlesNeedUpload)
	{
		UploadCarrierParticles(GraphBuilder, State, CarrierParticleBuffers[0]);
		UploadCarrierParticles(GraphBuilder, State, CarrierParticleBuffers[1]);
		State.CurrentCarrierParticleIndex = 0;
		State.bCarrierParticlesNeedUpload = false;
	}

	if (State.bVortexParticlesNeedUpload)
	{
		UploadDeadVortexParticles(GraphBuilder, State, VortexParticleBuffers[0]);
		UploadDeadVortexParticles(GraphBuilder, State, VortexParticleBuffers[1]);
		State.CurrentVortexParticleIndex = 0;
		State.bVortexParticlesNeedUpload = false;
	}

	const int32 CarrierReadIndex = State.CurrentCarrierParticleIndex;
	const int32 CarrierWriteIndex = 1 - State.CurrentCarrierParticleIndex;
	AddCarrierParticleUpdatePass(GraphBuilder, State, CarrierParticleBuffers[CarrierReadIndex], CarrierParticleBuffers[CarrierWriteIndex], SimulationEventBuffer, SimulationEventCount, DeltaSeconds);
	State.CurrentCarrierParticleIndex = CarrierWriteIndex;

	if (State.bNeedsInit)
	{
		AddInitPass(GraphBuilder, State, DensityTextures[State.CurrentDensityIndex], VelocityTextures[State.CurrentVelocityIndex]);
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(BulletCutoutTextures[0]), 0.0f);
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(BulletCutoutTextures[1]), 0.0f);
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(BulletSinkTextures[0]), 0.0f);
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(BulletSinkTextures[1]), 0.0f);
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(WarpTextures[0]), 0.0f);
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(WarpTextures[1]), 0.0f);
		State.bNeedsInit = false;
	}

	const int32 ReadDensityIndex = State.CurrentDensityIndex;
	const int32 ReadVelocityIndex = State.CurrentVelocityIndex;
	const int32 WriteDensityIndex = 1 - State.CurrentDensityIndex;
	const int32 WriteVelocityIndex = 1 - State.CurrentVelocityIndex;

	const int32 BulletReadIndex = State.CurrentBulletFieldIndex;
	const int32 BulletWriteIndex = 1 - State.CurrentBulletFieldIndex;
	AddBulletFieldPass(
		GraphBuilder,
		State,
		BulletCutoutTextures[BulletReadIndex],
		BulletSinkTextures[BulletReadIndex],
		VelocityTextures[ReadVelocityIndex],
		BulletCutoutTextures[BulletWriteIndex],
		BulletSinkTextures[BulletWriteIndex],
		BulletImpulseTexture,
		SimulationEventBuffer,
		SimulationEventCount,
		DeltaSeconds);
	State.CurrentBulletFieldIndex = BulletWriteIndex;

	AddSimulatePass(GraphBuilder, State, DensityTextures[ReadDensityIndex], VelocityTextures[ReadVelocityIndex], BulletCutoutTextures[State.CurrentBulletFieldIndex], BulletSinkTextures[State.CurrentBulletFieldIndex], BulletImpulseTexture, CarrierParticleBuffers[State.CurrentCarrierParticleIndex], DensityTextures[WriteDensityIndex], VelocityTextures[WriteVelocityIndex], DeltaSeconds);
	State.CurrentDensityIndex = WriteDensityIndex;
	State.CurrentVelocityIndex = WriteVelocityIndex;

	const int32 WarpReadIndex = State.CurrentWarpIndex;
	const int32 WarpWriteIndex = 1 - State.CurrentWarpIndex;
	AddWarpPass(GraphBuilder, State, DensityTextures[State.CurrentDensityIndex], WarpTextures[WarpReadIndex], WarpTextures[WarpWriteIndex], SimulationEventBuffer, SimulationEventCount, DeltaSeconds);
	State.CurrentWarpIndex = WarpWriteIndex;

	if (!State.PendingEvents.IsEmpty())
	{
		if (HasEventType(State.PendingEvents, ETimeThiefSmokeRendererInteractionType::ExplosionShock))
		{
			const int32 EventReadDensityIndex = State.CurrentDensityIndex;
			const int32 EventReadVelocityIndex = State.CurrentVelocityIndex;
			const int32 EventWriteDensityIndex = 1 - State.CurrentDensityIndex;
			const int32 EventWriteVelocityIndex = 1 - State.CurrentVelocityIndex;
			AddApplyEventsPass(GraphBuilder, State, DensityTextures[EventReadDensityIndex], VelocityTextures[EventReadVelocityIndex], DensityTextures[EventWriteDensityIndex], VelocityTextures[EventWriteVelocityIndex], SimulationEventBuffer, SimulationEventCount, DeltaSeconds);
			State.CurrentDensityIndex = EventWriteDensityIndex;
			State.CurrentVelocityIndex = EventWriteVelocityIndex;
		}

		if (HasEventType(State.PendingEvents, ETimeThiefSmokeRendererInteractionType::ActorPush))
		{
			const int32 ObstacleReadDensityIndex = State.CurrentDensityIndex;
			const int32 ObstacleReadVelocityIndex = State.CurrentVelocityIndex;
			const int32 ObstacleWriteDensityIndex = 1 - State.CurrentDensityIndex;
			const int32 ObstacleWriteVelocityIndex = 1 - State.CurrentVelocityIndex;
			AddDynamicObstaclePass(GraphBuilder, State, DensityTextures[ObstacleReadDensityIndex], VelocityTextures[ObstacleReadVelocityIndex], DensityTextures[ObstacleWriteDensityIndex], VelocityTextures[ObstacleWriteVelocityIndex], SimulationEventBuffer, SimulationEventCount);
			State.CurrentDensityIndex = ObstacleWriteDensityIndex;
			State.CurrentVelocityIndex = ObstacleWriteVelocityIndex;
		}
	}

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
		SimulationEventBuffer,
		SimulationEventCount,
		DeltaSeconds);
	State.CurrentVortexParticleIndex = VortexParticleWriteIndex;

	const int32 VorticityWriteVelocityIndex = 1 - State.CurrentVelocityIndex;
	AddVorticityPass(GraphBuilder, State, DensityTextures[State.CurrentDensityIndex], VelocityTextures[State.CurrentVelocityIndex], VelocityTextures[VorticityWriteVelocityIndex], SimulationEventBuffer, SimulationEventCount, DeltaSeconds);
	State.CurrentVelocityIndex = VorticityWriteVelocityIndex;

	const int32 VortexSplatWriteVelocityIndex = 1 - State.CurrentVelocityIndex;
	AddVortexParticleSplatPass(
		GraphBuilder,
		State,
		VortexParticleBuffers[State.CurrentVortexParticleIndex],
		DensityTextures[State.CurrentDensityIndex],
		VelocityTextures[State.CurrentVelocityIndex],
		BulletCutoutTextures[State.CurrentBulletFieldIndex],
		BulletSinkTextures[State.CurrentBulletFieldIndex],
		VelocityTextures[VortexSplatWriteVelocityIndex]);
	State.CurrentVelocityIndex = VortexSplatWriteVelocityIndex;
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
			WarpTextures[State.CurrentWarpIndex],
			BulletCutoutTextures[State.CurrentBulletFieldIndex],
			BulletSinkTextures[State.CurrentBulletFieldIndex],
			BrickActivityTexture);
		AddExpandBrickOccupancyPass(
			GraphBuilder,
			State,
			BrickActivityTexture,
			BrickOccupancyTexture);
		if (!State.bForceDenseComposite)
		{
			AddScatterSparseAtlasPass(
				GraphBuilder,
				State,
				DensityTextures[State.CurrentDensityIndex],
				WarpTextures[State.CurrentWarpIndex],
				BulletCutoutTextures[State.CurrentBulletFieldIndex],
				BulletSinkTextures[State.CurrentBulletFieldIndex],
				BrickOccupancyTexture);
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
	PassParameters->InitialDensity = State.Volume.Settings.InitialDensity;
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
	PassParameters->VolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->Events = GraphBuilder.CreateSRV(EventBuffer);
	PassParameters->OutDensity = GraphBuilder.CreateUAV(DensityOut);
	PassParameters->OutVelocity = GraphBuilder.CreateUAV(VelocityOut);
	PassParameters->GridResolution = GridSize;
	PassParameters->BoundsExtent = FVector3f(State.Volume.BoundsExtent);
	PassParameters->DeltaSeconds = DeltaSeconds;
	PassParameters->EventCount = EventCount;
	PassParameters->LocalToWorld = State.Volume.LocalToWorld.ToMatrixWithScale();

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.ApplyEvents SmokeId=%d Events=%d", State.Volume.SmokeId, EventCount),
		ComputeShader,
		PassParameters,
		GroupCount);
}

void FTimeThiefSmokeViewExtension::AddBulletFieldPass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGTextureRef CutoutIn,
	FRDGTextureRef SinkIn,
	FRDGTextureRef VelocityIn,
	FRDGTextureRef CutoutOut,
	FRDGTextureRef SinkOut,
	FRDGTextureRef BulletImpulseOut,
	FRDGBufferRef EventBuffer,
	int32 EventCount,
	float DeltaSeconds)
{
	const FIntVector GridSize = State.AllocatedGridSize;
	const FIntVector GroupCount = MakeGroupCount(GridSize);

	TShaderMapRef<FTimeThiefSmokeBulletSuppressCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokeBulletSuppressCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeBulletSuppressCS::FParameters>();
	PassParameters->CutoutIn = CutoutIn;
	PassParameters->SinkIn = SinkIn;
	PassParameters->VelocityIn = VelocityIn;
	PassParameters->VolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->Events = GraphBuilder.CreateSRV(EventBuffer);
	PassParameters->OutCutout = GraphBuilder.CreateUAV(CutoutOut);
	PassParameters->OutSink = GraphBuilder.CreateUAV(SinkOut);
	PassParameters->OutBulletImpulse = GraphBuilder.CreateUAV(BulletImpulseOut);
	PassParameters->GridResolution = GridSize;
	PassParameters->BoundsExtent = FVector3f(State.Volume.BoundsExtent);
	PassParameters->DeltaSeconds = DeltaSeconds;
	PassParameters->BulletWakeCutoutLife = FMath::Max(0.05f, State.Volume.Settings.BulletWakeMaxVisibleLife);
	PassParameters->BulletWakeReleaseDuration = FMath::Max(0.05f, State.Volume.Settings.BulletWakeReleaseDuration);
	PassParameters->BulletWakeSinkLife = FMath::Max(0.05f, State.Volume.Settings.BulletWakeSinkLife);
	PassParameters->BulletWakeSinkStrength = FMath::Max(0.0f, State.Volume.Settings.BulletWakeSinkStrength);
	PassParameters->BulletWakeImpulseStrength = State.Volume.Settings.BulletWakeImpulseStrength;
	PassParameters->BulletWakeCutoutFeather = FMath::Max(0.2f, State.Volume.Settings.BulletWakeCutoutFeather);
	PassParameters->EventCount = EventCount;
	PassParameters->LocalToWorld = State.Volume.LocalToWorld.ToMatrixWithScale();
	PassParameters->WorldToLocal = State.Volume.LocalToWorld.ToInverseMatrixWithScale();

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.BulletFields SmokeId=%d Events=%d", State.Volume.SmokeId, EventCount),
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
	int32 EventCount)
{
	const FIntVector GridSize = State.AllocatedGridSize;
	const FIntVector GroupCount = MakeGroupCount(GridSize);

	TShaderMapRef<FTimeThiefSmokeDynamicObstacleCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokeDynamicObstacleCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeDynamicObstacleCS::FParameters>();
	PassParameters->DensityIn = DensityIn;
	PassParameters->VelocityIn = VelocityIn;
	PassParameters->ObstacleTexture = GraphBuilder.RegisterExternalTexture(State.ObstacleTexture);
	PassParameters->VolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->Events = GraphBuilder.CreateSRV(EventBuffer);
	PassParameters->OutDensity = GraphBuilder.CreateUAV(DensityOut);
	PassParameters->OutVelocity = GraphBuilder.CreateUAV(VelocityOut);
	PassParameters->GridResolution = GridSize;
	PassParameters->BoundsExtent = FVector3f(State.Volume.BoundsExtent);
	PassParameters->EventCount = EventCount;
	PassParameters->LocalToWorld = State.Volume.LocalToWorld.ToMatrixWithScale();

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
	FRDGTextureRef BulletImpulseTexture,
	FRDGBufferRef CarrierBuffer,
	FRDGTextureRef DensityOut,
	FRDGTextureRef VelocityOut,
	float DeltaSeconds)
{
	const FIntVector GridSize = State.AllocatedGridSize;
	const FIntVector GroupCount = MakeGroupCount(GridSize);

	TShaderMapRef<FTimeThiefSmokeSimulateCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokeSimulateCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeSimulateCS::FParameters>();
	PassParameters->DensityIn = DensityIn;
	PassParameters->VelocityIn = VelocityIn;
	PassParameters->ObstacleTexture = GraphBuilder.RegisterExternalTexture(State.ObstacleTexture);
	PassParameters->BulletCutoutTexture = BulletCutoutTexture;
	PassParameters->BulletSinkTexture = BulletSinkTexture;
	PassParameters->BulletImpulseTexture = BulletImpulseTexture;
	PassParameters->VolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->CarrierParticles = GraphBuilder.CreateSRV(CarrierBuffer);
	PassParameters->OutDensity = GraphBuilder.CreateUAV(DensityOut);
	PassParameters->OutVelocity = GraphBuilder.CreateUAV(VelocityOut);
	PassParameters->GridResolution = GridSize;
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
	PassParameters->bUseMacCormackAdvection = State.Volume.Settings.bUseMacCormackAdvection ? 1u : 0u;
	PassParameters->CarrierParticleCount = FMath::Min(State.AllocatedCarrierParticleCount, MaxCarrierParticleCount);
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
	PassParameters->VolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->Events = GraphBuilder.CreateSRV(EventBuffer);
	PassParameters->OutWarp = GraphBuilder.CreateUAV(WarpOut);
	PassParameters->GridResolution = GridSize;
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

void FTimeThiefSmokeViewExtension::AddVorticityPass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGTextureRef DensityIn,
	FRDGTextureRef VelocityIn,
	FRDGTextureRef VelocityOut,
	FRDGBufferRef EventBuffer,
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
	PassParameters->ObstacleTexture = GraphBuilder.RegisterExternalTexture(State.ObstacleTexture);
	PassParameters->VolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->Events = GraphBuilder.CreateSRV(EventBuffer);
	PassParameters->OutVelocity = GraphBuilder.CreateUAV(VelocityOut);
	PassParameters->GridResolution = GridSize;
	PassParameters->BoundsExtent = FVector3f(State.Volume.BoundsExtent);
	PassParameters->CellSize = CellSize;
	PassParameters->DeltaSeconds = DeltaSeconds;
	PassParameters->AgeSeconds = State.Volume.AgeSeconds;
	PassParameters->VorticityConfinementStrength = State.Volume.Settings.VorticityConfinementStrength;
	PassParameters->TurbulenceStrength = State.Volume.Settings.TurbulenceStrength;
	PassParameters->AirInteractionStrength = State.Volume.Settings.AirInteractionStrength;
	PassParameters->EventVortexStrength = State.Volume.Settings.EventVortexStrength;
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
	FRDGTextureRef WarpTexture,
	FRDGTextureRef BulletCutoutTexture,
	FRDGTextureRef BulletSinkTexture,
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
	PassParameters->DensityTexture = DensityTexture;
	PassParameters->WarpTexture = WarpTexture;
	PassParameters->BulletCutoutTexture = BulletCutoutTexture;
	PassParameters->BulletSinkTexture = BulletSinkTexture;
	PassParameters->OutBrickOccupancy = GraphBuilder.CreateUAV(BrickActivityTexture);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.BuildBrickOccupancy SmokeId=%d", State.Volume.SmokeId),
		ComputeShader,
		PassParameters,
		GroupCount);
}

void FTimeThiefSmokeViewExtension::AddExpandBrickOccupancyPass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGTextureRef BrickActivityTexture,
	FRDGTextureRef BrickOccupancyTexture)
{
	const FIntVector BrickGridSize = State.AllocatedBrickGridSize;
	const FIntVector GroupCount = MakeGroupCount(BrickGridSize);

	TShaderMapRef<FTimeThiefSmokeExpandBrickOccupancyCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FRDGBufferRef BrickAllocatorBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1),
		TEXT("TimeThiefSmoke.BrickAllocator"));
	FRDGBufferUAVRef BrickAllocatorUAV = GraphBuilder.CreateUAV(BrickAllocatorBuffer);
	AddClearUAVPass(GraphBuilder, BrickAllocatorUAV, 0u);

	FTimeThiefSmokeExpandBrickOccupancyCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeExpandBrickOccupancyCS::FParameters>();
	PassParameters->BrickGridResolution = BrickGridSize;
	PassParameters->MaxActiveSmokeBricks = FMath::Max(State.Volume.Settings.MaxActiveSmokeBricks, 1);
	PassParameters->BrickActivityTexture = BrickActivityTexture;
	PassParameters->BrickAllocator = BrickAllocatorUAV;
	PassParameters->OutBrickOccupancy = GraphBuilder.CreateUAV(BrickOccupancyTexture);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.ExpandBrickOccupancy SmokeId=%d", State.Volume.SmokeId),
		ComputeShader,
		PassParameters,
		GroupCount);

	if (!State.ActiveBrickCountReadback.IsValid())
	{
		State.ActiveBrickCountReadback = MakeShared<FRHIGPUBufferReadback>(TEXT("TimeThiefSmoke.ActiveBrickCountReadback"));
	}
	if (!State.bActiveBrickCountReadbackPending)
	{
		AddEnqueueCopyPass(GraphBuilder, State.ActiveBrickCountReadback.Get(), BrickAllocatorBuffer, sizeof(uint32));
		State.bActiveBrickCountReadbackPending = true;
	}
}

void FTimeThiefSmokeViewExtension::AddScatterSparseAtlasPass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGTextureRef DensityTexture,
	FRDGTextureRef WarpTexture,
	FRDGTextureRef BulletCutoutTexture,
	FRDGTextureRef BulletSinkTexture,
	FRDGTextureRef BrickOccupancyTexture)
{
	const int32 BrickSize = FMath::Clamp(State.Volume.Settings.SmokeBrickSize, 4, 32);
	const FIntVector GridSize = State.AllocatedGridSize;
	const FIntVector GroupCount = MakeGroupCount(GridSize);

	TShaderMapRef<FTimeThiefSmokeScatterSparseAtlasCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokeScatterSparseAtlasCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeScatterSparseAtlasCS::FParameters>();
	PassParameters->GridResolution = GridSize;
	PassParameters->BrickGridResolution = State.AllocatedBrickGridSize;
	PassParameters->SparseAtlasBrickGridResolution = State.AllocatedSparseAtlasBrickGridSize;
	PassParameters->SmokeBrickSize = BrickSize;
	PassParameters->MaxActiveSmokeBricks = FMath::Max(State.Volume.Settings.MaxActiveSmokeBricks, 1);
	PassParameters->DensityTexture = DensityTexture;
	PassParameters->WarpTexture = WarpTexture;
	PassParameters->BulletCutoutTexture = BulletCutoutTexture;
	PassParameters->BulletSinkTexture = BulletSinkTexture;
	PassParameters->BrickOccupancyTexture = BrickOccupancyTexture;
	PassParameters->OutSparseDensityAtlas = GraphBuilder.CreateUAV(GraphBuilder.RegisterExternalTexture(State.SparseDensityAtlasTexture));
	PassParameters->OutSparseWarpAtlas = GraphBuilder.CreateUAV(GraphBuilder.RegisterExternalTexture(State.SparseWarpAtlasTexture));
	PassParameters->OutSparseBulletCutoutAtlas = GraphBuilder.CreateUAV(GraphBuilder.RegisterExternalTexture(State.SparseBulletCutoutAtlasTexture));
	PassParameters->OutSparseBulletSinkAtlas = GraphBuilder.CreateUAV(GraphBuilder.RegisterExternalTexture(State.SparseBulletSinkAtlasTexture));

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
