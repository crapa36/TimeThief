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
#include "SystemTextures.h"
#include "TimeThiefSmokeParameterDefaults.h"
#include "TimeThiefSmokeShaders.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogTimeThiefSmokeRenderer, Log, All);

#define TIME_THIEF_SMOKE_SET_OBSTACLE_FIELD_PARAMETERS(PassParameters, SdfTexture, GraphBuilder, State) \
	do \
	{ \
		(PassParameters)->ObstacleTexture = (SdfTexture); \
		(PassParameters)->ObstacleVelocityTexture = (GraphBuilder).RegisterExternalTexture((State).ObstacleVelocityTexture); \
		(PassParameters)->ObstacleFaceOpenTexture = (GraphBuilder).RegisterExternalTexture((State).ObstacleFaceOpenTexture); \
		(PassParameters)->ObstacleSdfSurfaceFeatherCm = TimeThiefSmokeParameterDefaults::ObstacleSdfSurfaceFeatherCm; \
		(PassParameters)->VolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(); \
	} while (false)

namespace
{
	static TAutoConsoleVariable<int32> CVarTimeThiefSmokeScissor(
		TEXT("r.TimeThiefSmoke.Scissor"),
		TimeThiefSmokeParameterDefaults::bUseCompositeScissorByDefault,
		TEXT("Limits custom smoke composite draw calls to each smoke AABB screen rect when enabled."));

	static TAutoConsoleVariable<int32> CVarTimeThiefSmokeFastFilament(
		TEXT("r.TimeThiefSmoke.FastFilament"),
		TimeThiefSmokeParameterDefaults::bUseFastFilamentByDefault,
		TEXT("Uses the cheaper filament shading branch for custom smoke. 0=full, 1=fast."));

	static TAutoConsoleVariable<float> CVarTimeThiefSmokeSimulationHz(
		TEXT("r.TimeThiefSmoke.SimulationHz"),
		TimeThiefSmokeParameterDefaults::SimulationHz,
		TEXT("Caps custom smoke simulation update rate. <=0 simulates every rendered frame."));

	uint64 GetSceneKey(const FSceneViewFamily& ViewFamily)
	{
		return reinterpret_cast<uint64>(ViewFamily.Scene);
	}

	uint64 GetSceneKey(const FSceneView& View)
	{
		return View.Family ? GetSceneKey(*View.Family) : 0;
	}

	int32 MakeAxisGridSize(const float AxisExtent, const float MaxExtent, const int32 MaxAxisResolution)
	{
		if (MaxExtent <= UE_SMALL_NUMBER)
		{
			return MaxAxisResolution;
		}

		const float AxisRatio = FMath::Clamp(AxisExtent / MaxExtent, 0.0f, 1.0f);
		const int32 RawResolution = FMath::RoundToInt(static_cast<float>(MaxAxisResolution) * AxisRatio);
		const int32 AlignedResolution = FMath::DivideAndRoundUp(FMath::Max(RawResolution, TimeThiefSmokeParameterDefaults::SmokeGridMinAxisResolution), TimeThiefSmokeParameterDefaults::SmokeThreadGroupSize) * TimeThiefSmokeParameterDefaults::SmokeThreadGroupSize;
		return FMath::Clamp(AlignedResolution, TimeThiefSmokeParameterDefaults::SmokeGridMinAxisResolution, MaxAxisResolution);
	}

	FIntVector MakeGridSize(const int32 Resolution, const FVector3f& BoundsExtent)
	{
		const int32 MaxAxisResolution = FMath::Clamp(Resolution, TimeThiefSmokeParameterDefaults::SmokeGridMinAxisResolution, TimeThiefSmokeParameterDefaults::SmokeGridMaxAxisResolution);
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
			FMath::DivideAndRoundUp(GridSize.X, TimeThiefSmokeParameterDefaults::SmokeThreadGroupSize),
			FMath::DivideAndRoundUp(GridSize.Y, TimeThiefSmokeParameterDefaults::SmokeThreadGroupSize),
			FMath::DivideAndRoundUp(GridSize.Z, TimeThiefSmokeParameterDefaults::SmokeThreadGroupSize));
	}

	uint64 GetVoxelCount(const FIntVector& GridSize)
	{
		return static_cast<uint64>(FMath::Max(GridSize.X, 1)) *
			static_cast<uint64>(FMath::Max(GridSize.Y, 1)) *
			static_cast<uint64>(FMath::Max(GridSize.Z, 1));
	}

	bool HasRenderableSparseState(const uint32 SparseActiveBrickCount, const bool bReadbackPending)
	{
		return SparseActiveBrickCount > 0u || bReadbackPending;
	}

	bool ShouldUseSparseComposite(const FIntVector& BrickGridSize, const uint32 SparseActiveBrickCount, const uint32 MaxActiveSmokeBricks)
	{
		if (SparseActiveBrickCount == 0u || MaxActiveSmokeBricks == 0u)
		{
			return false;
		}

		const uint64 TotalBrickCount = GetVoxelCount(BrickGridSize);
		const uint64 ActiveBrickCount = static_cast<uint64>(SparseActiveBrickCount);
		const uint64 MaxSparseCompositeBrickCount = FMath::Max<uint64>(
			1u,
			FMath::CeilToInt64(static_cast<double>(TotalBrickCount) * static_cast<double>(TimeThiefSmokeParameterDefaults::SparseCompositeMaxActiveRatio)));
		return TotalBrickCount > 1u &&
			ActiveBrickCount < TotalBrickCount &&
			ActiveBrickCount <= static_cast<uint64>(MaxActiveSmokeBricks) &&
			ActiveBrickCount <= MaxSparseCompositeBrickCount;
	}

	bool ShouldUseFullscreenComposite(const FIntRect& SmokeRect, const FIntRect& ViewRect)
	{
		const int64 ViewArea = static_cast<int64>(FMath::Max(0, ViewRect.Width())) * static_cast<int64>(FMath::Max(0, ViewRect.Height()));
		const int64 SmokeArea = static_cast<int64>(FMath::Max(0, SmokeRect.Width())) * static_cast<int64>(FMath::Max(0, SmokeRect.Height()));
		return ViewArea <= 0 || static_cast<float>(SmokeArea) >= static_cast<float>(ViewArea) * TimeThiefSmokeParameterDefaults::CompositeFullscreenAreaThreshold;
	}

	bool CompositeRectsOverlap(const FIntRect& A, const FIntRect& B)
	{
		return A.Min.X < B.Max.X &&
			A.Max.X > B.Min.X &&
			A.Min.Y < B.Max.Y &&
			A.Max.Y > B.Min.Y;
	}

	uint64 MakeCompositePairKey(const int32 A, const int32 B)
	{
		const uint32 Low = static_cast<uint32>(FMath::Min(A, B));
		const uint32 High = static_cast<uint32>(FMath::Max(A, B));
		return (static_cast<uint64>(Low) << 32) | static_cast<uint64>(High);
	}

	struct FCompositeGroupUnionFind
	{
		TArray<int32> Parents;

		explicit FCompositeGroupUnionFind(const int32 Count)
		{
			Parents.SetNumUninitialized(Count);
			for (int32 Index = 0; Index < Count; ++Index)
			{
				Parents[Index] = Index;
			}
		}

		int32 Find(const int32 Index)
		{
			if (!Parents.IsValidIndex(Index))
			{
				return INDEX_NONE;
			}

			int32 Root = Index;
			while (Parents.IsValidIndex(Root) && Parents[Root] != Root)
			{
				Root = Parents[Root];
			}

			int32 Current = Index;
			while (Parents.IsValidIndex(Current) && Parents[Current] != Root)
			{
				const int32 Next = Parents[Current];
				Parents[Current] = Root;
				Current = Next;
			}

			return Root;
		}

		void Union(const int32 A, const int32 B)
		{
			const int32 RootA = Find(A);
			const int32 RootB = Find(B);
			if (RootA != INDEX_NONE && RootB != INDEX_NONE && RootA != RootB)
			{
				Parents[RootB] = RootA;
			}
		}
	};

	void BuildCompositeOverlapGroups(
		const TArray<FIntRect>& CandidateRects,
		const TArray<uint8>& CandidateValidFlags,
		const FIntRect& SceneRect,
		TMap<int32, TArray<int32>>& OutGroupIndicesByRoot,
		int32* OutTestedPairCount = nullptr)
	{
		OutGroupIndicesByRoot.Reset();
		if (OutTestedPairCount)
		{
			*OutTestedPairCount = 0;
		}

		FCompositeGroupUnionFind Groups(CandidateRects.Num());
		TMap<FIntPoint, TArray<int32>> TileCandidateIndices;
		TSet<uint64> TestedPairKeys;
		const int32 CompositeTileSize = FMath::Max(TimeThiefSmokeParameterDefaults::CompositeTileSize, 1);
		const FIntPoint TileGridSize(
			FMath::Max(FMath::DivideAndRoundUp(SceneRect.Width(), CompositeTileSize), 1),
			FMath::Max(FMath::DivideAndRoundUp(SceneRect.Height(), CompositeTileSize), 1));

		for (int32 CandidateIndex = 0; CandidateIndex < CandidateRects.Num(); ++CandidateIndex)
		{
			if (!CandidateValidFlags.IsValidIndex(CandidateIndex) || CandidateValidFlags[CandidateIndex] == 0)
			{
				continue;
			}

			const FIntRect& Rect = CandidateRects[CandidateIndex];
			const int32 MinTileX = FMath::Clamp((Rect.Min.X - SceneRect.Min.X) / CompositeTileSize, 0, TileGridSize.X - 1);
			const int32 MinTileY = FMath::Clamp((Rect.Min.Y - SceneRect.Min.Y) / CompositeTileSize, 0, TileGridSize.Y - 1);
			const int32 MaxTileX = FMath::Clamp((Rect.Max.X - 1 - SceneRect.Min.X) / CompositeTileSize, 0, TileGridSize.X - 1);
			const int32 MaxTileY = FMath::Clamp((Rect.Max.Y - 1 - SceneRect.Min.Y) / CompositeTileSize, 0, TileGridSize.Y - 1);
			for (int32 TileY = MinTileY; TileY <= MaxTileY; ++TileY)
			{
				for (int32 TileX = MinTileX; TileX <= MaxTileX; ++TileX)
				{
					TArray<int32>& TileCandidates = TileCandidateIndices.FindOrAdd(FIntPoint(TileX, TileY));
					for (const int32 OtherCandidateIndex : TileCandidates)
					{
						const uint64 PairKey = MakeCompositePairKey(CandidateIndex, OtherCandidateIndex);
						if (TestedPairKeys.Contains(PairKey))
						{
							continue;
						}

						TestedPairKeys.Add(PairKey);
						if (CompositeRectsOverlap(Rect, CandidateRects[OtherCandidateIndex]))
						{
							Groups.Union(CandidateIndex, OtherCandidateIndex);
						}
					}
					TileCandidates.Add(CandidateIndex);
				}
			}
		}

		if (OutTestedPairCount)
		{
			*OutTestedPairCount = TestedPairKeys.Num();
		}

		for (int32 CandidateIndex = 0; CandidateIndex < CandidateRects.Num(); ++CandidateIndex)
		{
			if (!CandidateValidFlags.IsValidIndex(CandidateIndex) || CandidateValidFlags[CandidateIndex] == 0)
			{
				continue;
			}

			const int32 Root = Groups.Find(CandidateIndex);
			if (Root != INDEX_NONE)
			{
				OutGroupIndicesByRoot.FindOrAdd(Root).Add(CandidateIndex);
			}
		}
	}

	FVector3f MakeCellSize(const FTimeThiefSmokeRendererVolume& Volume, const FIntVector& GridSize)
	{
		const FVector3f GridSizeFloat(
			static_cast<float>(FMath::Max(GridSize.X, 1)),
			static_cast<float>(FMath::Max(GridSize.Y, 1)),
			static_cast<float>(FMath::Max(GridSize.Z, 1)));
		return (FVector3f(Volume.BoundsExtent) * 2.0f) / GridSizeFloat;
	}

	FIntVector MakeBrickGridSize(const FIntVector& GridSize, const int32 BrickSize)
	{
		const int32 SafeBrickSize = FMath::Max(BrickSize, 1);
		return FIntVector(
			FMath::Max(1, FMath::DivideAndRoundUp(GridSize.X, SafeBrickSize)),
			FMath::Max(1, FMath::DivideAndRoundUp(GridSize.Y, SafeBrickSize)),
			FMath::Max(1, FMath::DivideAndRoundUp(GridSize.Z, SafeBrickSize)));
	}

	uint32 GetBrickGridCount(const FIntVector& BrickGridSize)
	{
		return static_cast<uint32>(FMath::Max(1, BrickGridSize.X * BrickGridSize.Y * BrickGridSize.Z));
	}

	FIntVector MakeSparseAtlasBrickGridSize(const int32 MaxActiveBricks)
	{
		const int32 SafeMaxActiveBricks = FMath::Max(MaxActiveBricks, 1);
		const int32 Axis = FMath::Max(1, FMath::CeilToInt(FMath::Pow(static_cast<float>(SafeMaxActiveBricks), 1.0f / 3.0f)));
		const int32 SliceCount = FMath::Max(1, FMath::DivideAndRoundUp(SafeMaxActiveBricks, Axis * Axis));
		return FIntVector(Axis, Axis, SliceCount);
	}

	uint32 ComputeSparseScatterGroupsPerBrick(const int32 BrickSize)
	{
		const int32 SafeBrickSize = FMath::Clamp(BrickSize, TimeThiefSmokeParameterDefaults::SmokeBrickMinSize, TimeThiefSmokeParameterDefaults::SmokeBrickMaxSize);
		const uint32 GroupsPerBrickAxis = static_cast<uint32>(FMath::DivideAndRoundUp(SafeBrickSize, TimeThiefSmokeParameterDefaults::SmokeThreadGroupSize));
		return FMath::Max(GroupsPerBrickAxis * GroupsPerBrickAxis * GroupsPerBrickAxis, 1u);
	}

	FIntVector MakeSparseAtlasGridSize(const FIntVector& AtlasBrickGridSize, const int32 BrickSize)
	{
		const int32 SafeBrickSize = FMath::Max(BrickSize, 1);
		return FIntVector(
			FMath::Max(1, AtlasBrickGridSize.X * SafeBrickSize),
			FMath::Max(1, AtlasBrickGridSize.Y * SafeBrickSize),
			FMath::Max(1, AtlasBrickGridSize.Z * SafeBrickSize));
	}

	ETimeThiefSmokeSimulationBackend GetDefaultSimulationBackend()
	{
		return TimeThiefSmokeParameterDefaults::bUseSparseMacSimulationByDefault
			? ETimeThiefSmokeSimulationBackend::SparseMac
			: ETimeThiefSmokeSimulationBackend::DenseLegacy;
	}

	FIntVector MakeStableGridSize(const FTimeThiefSmokeRendererVolume& Volume)
	{
		const int32 RequestedResolution = FMath::Clamp(TimeThiefSmokeParameterDefaults::SmokeGridResolution, TimeThiefSmokeParameterDefaults::SmokeGridMinAxisResolution, TimeThiefSmokeParameterDefaults::SmokeGridMaxAxisResolution);
		return MakeGridSize(RequestedResolution, Volume.BoundsExtent);
	}

	int32 QuantizeAllocatedGridAxis(const int32 RequiredAxis)
	{
		const int32 Quantum = FMath::Max(
			TimeThiefSmokeParameterDefaults::SmokeGridAllocationQuantum,
			TimeThiefSmokeParameterDefaults::SmokeThreadGroupSize);
		return FMath::Clamp(
			FMath::DivideAndRoundUp(FMath::Max(RequiredAxis, TimeThiefSmokeParameterDefaults::SmokeGridMinAxisResolution), Quantum) * Quantum,
			TimeThiefSmokeParameterDefaults::SmokeGridMinAxisResolution,
			TimeThiefSmokeParameterDefaults::SmokeGridMaxAxisResolution);
	}

	FIntVector MakeAllocatedGridSize(const FIntVector& RequiredGridSize)
	{
		return FIntVector(
			QuantizeAllocatedGridAxis(RequiredGridSize.X),
			QuantizeAllocatedGridAxis(RequiredGridSize.Y),
			QuantizeAllocatedGridAxis(RequiredGridSize.Z));
	}

	bool GridContains(const FIntVector& AllocatedGridSize, const FIntVector& RequiredGridSize)
	{
		return AllocatedGridSize.X >= RequiredGridSize.X &&
			AllocatedGridSize.Y >= RequiredGridSize.Y &&
			AllocatedGridSize.Z >= RequiredGridSize.Z;
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
			Event.NormalizedAge,
			static_cast<float>(Event.Seed));
		ShaderEvent.PreviousPositionSpeed = FVector4f(Event.PreviousPosition.X, Event.PreviousPosition.Y, Event.PreviousPosition.Z, Event.Speed);
		return ShaderEvent;
	}

	bool IsSimulationEventActive(const FTimeThiefSmokeRendererEvent& Event)
	{
		return Event.Strength > TimeThiefSmokeParameterDefaults::SimulationEventMinStrength;
	}

	bool IsDynamicSimulationEventActive(const FTimeThiefSmokeRendererEvent& Event)
	{
		return Event.Type != ETimeThiefSmokeRendererInteractionType::PlumeSource &&
			IsSimulationEventActive(Event);
	}

	bool IsVortexPipelineEvent(const FTimeThiefSmokeRendererEvent& Event)
	{
		if (!IsSimulationEventActive(Event))
		{
			return false;
		}

		switch (Event.Type)
		{
		case ETimeThiefSmokeRendererInteractionType::BulletWake:
		case ETimeThiefSmokeRendererInteractionType::ExplosionShock:
		case ETimeThiefSmokeRendererInteractionType::ActorPush:
			return true;
		case ETimeThiefSmokeRendererInteractionType::PlumeSource:
		default:
			return false;
		}
	}

	bool HasActiveSimulationEvent(const TArray<FTimeThiefSmokeRendererEvent>& Events)
	{
		for (const FTimeThiefSmokeRendererEvent& Event : Events)
		{
			if (IsSimulationEventActive(Event))
			{
				return true;
			}
		}
		return false;
	}

	bool HasActiveDynamicSimulationEvent(const TArray<FTimeThiefSmokeRendererEvent>& Events)
	{
		for (const FTimeThiefSmokeRendererEvent& Event : Events)
		{
			if (IsDynamicSimulationEventActive(Event))
			{
				return true;
			}
		}
		return false;
	}

	bool ShouldIncludeSmokeRenderCandidate(
		const ETimeThiefSmokeSimulationBackend SimulationBackend,
		const bool bNeedsInit,
		const uint32 SparseActiveBrickCount,
		const bool bSparseActiveBrickCountReadbackPending)
	{
		if (SimulationBackend != ETimeThiefSmokeSimulationBackend::SparseMac)
		{
			return true;
		}

		return bNeedsInit ||
			HasRenderableSparseState(SparseActiveBrickCount, bSparseActiveBrickCountReadbackPending);
	}

	bool IsPastSparseRenderableLifetime(const FTimeThiefSmokeRendererVolume& Volume)
	{
		return Volume.AgeSeconds >
			Volume.DurationSeconds +
			FMath::Max(TimeThiefSmokeParameterDefaults::SmokeFadeOutDuration, 0.0f) +
			UE_SMALL_NUMBER;
	}

	bool ShouldRefreshSparseSimulationMask(
		const bool bUseSparseSimulationMask,
		const bool bHasDynamicEvents,
		const uint32 SparseActiveBrickCount,
		const bool bSparseActiveBrickCountReadbackPending,
		const bool bSparseOccupancyRefreshPending)
	{
		return bUseSparseSimulationMask &&
			(bHasDynamicEvents || !HasRenderableSparseState(SparseActiveBrickCount, bSparseActiveBrickCountReadbackPending) || bSparseOccupancyRefreshPending);
	}

	bool ShouldRunVortexPasses(
		const bool bNeedsInit,
		const bool bVortexUploadPending,
		const int32 VortexEventCount,
		const float AccumulatedVortexDeltaSeconds)
	{
		return bNeedsInit ||
			bVortexUploadPending ||
			VortexEventCount > 0 ||
			AccumulatedVortexDeltaSeconds >= TimeThiefSmokeParameterDefaults::VortexSubstepIntervalSeconds;
	}

	float ComputeSmokeEventPriority(const FTimeThiefSmokeRendererEvent& Event)
	{
		const float Strength = FMath::Max(Event.Strength, TimeThiefSmokeParameterDefaults::EventPriorityMinStrength);
		const float AgeWeight = FMath::Max(1.0f - FMath::Clamp(Event.NormalizedAge, 0.0f, 1.0f), TimeThiefSmokeParameterDefaults::EventPriorityMinAgeWeight);
		const float RadiusWeight = FMath::Clamp(
			Event.Radius / TimeThiefSmokeParameterDefaults::EventPriorityRadiusDivisor,
			TimeThiefSmokeParameterDefaults::EventPriorityRadiusMin,
			TimeThiefSmokeParameterDefaults::EventPriorityRadiusMax);
		float TypeWeight = 1.0f;
		switch (Event.Type)
		{
		case ETimeThiefSmokeRendererInteractionType::ExplosionShock:
			TypeWeight = TimeThiefSmokeParameterDefaults::ExplosionEventPriorityWeight;
			break;
		case ETimeThiefSmokeRendererInteractionType::ActorPush:
			TypeWeight = TimeThiefSmokeParameterDefaults::ActorEventPriorityWeight;
			break;
		case ETimeThiefSmokeRendererInteractionType::PlumeSource:
			TypeWeight = TimeThiefSmokeParameterDefaults::PlumeEventPriorityWeight;
			break;
		case ETimeThiefSmokeRendererInteractionType::BulletWake:
			TypeWeight = TimeThiefSmokeParameterDefaults::BulletEventPriorityWeight;
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
		const int32 ClampedMaxEventCount = FMath::Clamp(MaxEventCount, 0, TimeThiefSmokeParameterDefaults::MaxShaderEventCount);
		if (Events.Num() > ClampedMaxEventCount)
		{
			Events.SetNum(ClampedMaxEventCount, EAllowShrinking::No);
		}
	}

	void ClampSmokeEventsByType(
		TArray<FTimeThiefSmokeRendererEvent>& Events,
		const ETimeThiefSmokeRendererInteractionType Type,
		const int32 MaxTypeEventCount)
	{
		int32 TypeEventCount = 0;
		for (int32 EventIndex = 0; EventIndex < Events.Num(); ++EventIndex)
		{
			if (Events[EventIndex].Type != Type)
			{
				continue;
			}

			++TypeEventCount;
			if (TypeEventCount > MaxTypeEventCount)
			{
				Events.RemoveAt(EventIndex, 1, EAllowShrinking::No);
				--EventIndex;
			}
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

	FRDGBufferRef GetEmptySmokeEventBuffer(FRDGBuilder& GraphBuilder, FRDGBufferRef& EmptyEventBuffer)
	{
		if (!EmptyEventBuffer)
		{
			TArray<FTimeThiefSmokeEventShaderData> EmptyEvents;
			EmptyEvents.AddDefaulted();
			EmptyEventBuffer = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateStructuredDesc(sizeof(FTimeThiefSmokeEventShaderData), EmptyEvents.Num()),
				TEXT("TimeThiefSmoke.EmptyEvents"));
			GraphBuilder.QueueBufferUpload(EmptyEventBuffer, EmptyEvents.GetData(), EmptyEvents.Num() * sizeof(FTimeThiefSmokeEventShaderData));
		}
		return EmptyEventBuffer;
	}

	FRDGBufferRef CreateSmokeEventBuffer(
		FRDGBuilder& GraphBuilder,
		const TArray<FTimeThiefSmokeRendererEvent>& Events,
		int32& OutEventCount,
		FRDGBufferRef& EmptyEventBuffer,
		const TCHAR* Name)
	{
		OutEventCount = FMath::Min(Events.Num(), TimeThiefSmokeParameterDefaults::MaxShaderEventCount);
		if (OutEventCount <= 0)
		{
			return GetEmptySmokeEventBuffer(GraphBuilder, EmptyEventBuffer);
		}

		TArray<FTimeThiefSmokeEventShaderData> ShaderEvents;
		ShaderEvents.Reserve(OutEventCount);
		for (int32 EventIndex = 0; EventIndex < OutEventCount; ++EventIndex)
		{
			ShaderEvents.Add(ToShaderEvent(Events[EventIndex]));
		}

		FRDGBufferRef EventBuffer = GraphBuilder.CreateBuffer(
			FRDGBufferDesc::CreateStructuredDesc(sizeof(FTimeThiefSmokeEventShaderData), ShaderEvents.Num()),
			Name);
		GraphBuilder.QueueBufferUpload(EventBuffer, ShaderEvents.GetData(), ShaderEvents.Num() * sizeof(FTimeThiefSmokeEventShaderData));
		return EventBuffer;
	}

	FRDGBufferRef GetEmptyBrickMaskBuffer(FRDGBuilder& GraphBuilder, FRDGBufferRef& EmptyBrickMaskBuffer)
	{
		if (!EmptyBrickMaskBuffer)
		{
			TArray<uint32> EmptyMask;
			EmptyMask.AddZeroed(4);
			EmptyBrickMaskBuffer = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32) * 4, 1),
				TEXT("TimeThiefSmoke.EmptyEventBrickMasks"));
			GraphBuilder.QueueBufferUpload(EmptyBrickMaskBuffer, EmptyMask.GetData(), EmptyMask.Num() * sizeof(uint32));
		}
		return EmptyBrickMaskBuffer;
	}

	void CreateEmptyActiveBrickBuffers(FRDGBuilder& GraphBuilder, FRDGBufferRef& OutActiveBrickCountBuffer, FRDGBufferRef& OutActiveBricksBuffer)
	{
		uint32 EmptyCount = 0u;
		OutActiveBrickCountBuffer = GraphBuilder.CreateBuffer(
			FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1),
			TEXT("TimeThiefSmoke.EmptyActiveBrickCount"));
		GraphBuilder.QueueBufferUpload(OutActiveBrickCountBuffer, &EmptyCount, sizeof(uint32));

		TArray<uint32> EmptyBrick;
		EmptyBrick.AddZeroed(4);
		OutActiveBricksBuffer = GraphBuilder.CreateBuffer(
			FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32) * 4, 1),
			TEXT("TimeThiefSmoke.EmptyActiveBricks"));
		GraphBuilder.QueueBufferUpload(OutActiveBricksBuffer, EmptyBrick.GetData(), EmptyBrick.Num() * sizeof(uint32));
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
		FBox WorldBounds(EForceInit::ForceInit);
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
					WorldBounds += LocalToWorld.TransformPosition(LocalCorner);
				}
			}
		}
		if (!WorldBounds.IsValid || !View.ViewFrustum.IntersectBox(WorldBounds.GetCenter(), WorldBounds.GetExtent()))
		{
			return false;
		}

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

		const int32 RectMinX = FMath::Clamp(FMath::FloorToInt(MinX) - TimeThiefSmokeParameterDefaults::CompositeScreenRectPadding, ViewRect.Min.X, ViewRect.Max.X);
		const int32 RectMinY = FMath::Clamp(FMath::FloorToInt(MinY) - TimeThiefSmokeParameterDefaults::CompositeScreenRectPadding, ViewRect.Min.Y, ViewRect.Max.Y);
		const int32 RectMaxX = FMath::Clamp(FMath::CeilToInt(MaxX) + TimeThiefSmokeParameterDefaults::CompositeScreenRectPadding, ViewRect.Min.X, ViewRect.Max.X);
		const int32 RectMaxY = FMath::Clamp(FMath::CeilToInt(MaxY) + TimeThiefSmokeParameterDefaults::CompositeScreenRectPadding, ViewRect.Min.Y, ViewRect.Max.Y);
		OutRect = FIntRect(RectMinX, RectMinY, RectMaxX, RectMaxY);
		return OutRect.Width() > 0 && OutRect.Height() > 0;
	}

	bool IsSmokeVisibleInViewFamily(const FSceneViewFamily& ViewFamily, const FTimeThiefSmokeRendererVolume& Volume)
	{
		for (const FSceneView* View : ViewFamily.Views)
		{
			if (!View)
			{
				continue;
			}

			FIntRect SmokeRect;
			if (ComputeSmokeScreenRect(*View, Volume, View->UnscaledViewRect, SmokeRect))
			{
				return true;
			}
		}
		return false;
	}

#if WITH_DEV_AUTOMATION_TESTS
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FTimeThiefSmokeCompositeGroupBroadphaseAutomationTest,
		"TimeThief.Smoke.Renderer.CompositeGroupBroadphase",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FTimeThiefSmokeCompositeGroupBroadphaseAutomationTest::RunTest(const FString& Parameters)
	{
		const FIntRect SceneRect(0, 0, 4096, 4096);
		TArray<FIntRect> FarRects;
		TArray<uint8> FarValidFlags;
		for (int32 Index = 0; Index < 64; ++Index)
		{
			const int32 X = (Index % 8) * 512;
			const int32 Y = (Index / 8) * 512;
			FarRects.Add(FIntRect(X, Y, X + 64, Y + 64));
			FarValidFlags.Add(1);
		}

		TMap<int32, TArray<int32>> Groups;
		int32 TestedPairCount = 0;
		BuildCompositeOverlapGroups(FarRects, FarValidFlags, SceneRect, Groups, &TestedPairCount);
		TestEqual(TEXT("Separated composite rects remain single groups"), Groups.Num(), FarRects.Num());
		TestEqual(TEXT("Separated composite rects emit no candidate pair tests"), TestedPairCount, 0);

		TArray<FIntRect> OverlapRects;
		TArray<uint8> OverlapValidFlags;
		OverlapRects.Add(FIntRect(100, 100, 260, 260));
		OverlapRects.Add(FIntRect(220, 180, 360, 320));
		OverlapRects.Add(FIntRect(900, 900, 980, 980));
		OverlapValidFlags.Init(1, OverlapRects.Num());

		Groups.Reset();
		TestedPairCount = 0;
		BuildCompositeOverlapGroups(OverlapRects, OverlapValidFlags, SceneRect, Groups, &TestedPairCount);
		TestEqual(TEXT("Overlapping composite rects merge into two groups"), Groups.Num(), 2);
		TestTrue(TEXT("Overlapping composite broadphase tests at least one pair"), TestedPairCount >= 1);

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FTimeThiefSmokeSimulationEventSelectionAutomationTest,
		"TimeThief.Smoke.Renderer.SimulationEventSelection",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FTimeThiefSmokeSimulationEventSelectionAutomationTest::RunTest(const FString& Parameters)
	{
		FTimeThiefSmokeRendererEvent WeakEvent;
		WeakEvent.Strength = TimeThiefSmokeParameterDefaults::SimulationEventMinStrength * 0.5f;
		TestFalse(TEXT("Weak events do not wake simulation passes"), IsSimulationEventActive(WeakEvent));

		FTimeThiefSmokeRendererEvent StrongEvent;
		StrongEvent.Strength = TimeThiefSmokeParameterDefaults::SimulationEventMinStrength * 2.0f;
		TestTrue(TEXT("Strong events wake simulation passes"), IsSimulationEventActive(StrongEvent));

		TestFalse(
			TEXT("Idle sparse smoke reuses the previous simulation mask"),
			ShouldRefreshSparseSimulationMask(
				true,
				false,
				8u,
				false,
				false));
		TestTrue(
			TEXT("Sparse smoke with active events refreshes the simulation mask"),
			ShouldRefreshSparseSimulationMask(
				true,
				true,
				8u,
				false,
				false));
		TestTrue(
			TEXT("Sparse smoke with no renderable sparse state refreshes the simulation mask"),
			ShouldRefreshSparseSimulationMask(
				true,
				false,
				0u,
				false,
				false));
		TestTrue(
			TEXT("Sparse smoke with deferred occupancy refresh scans before the next simulation"),
			ShouldRefreshSparseSimulationMask(
				true,
				false,
				8u,
				false,
				true));
		FTimeThiefSmokeRendererEvent UrgentExplosionEvent;
		UrgentExplosionEvent.Type = ETimeThiefSmokeRendererInteractionType::ExplosionShock;
		UrgentExplosionEvent.Strength = 1.0f;
		TArray<FTimeThiefSmokeRendererEvent> DynamicEvents;
		DynamicEvents.Add(UrgentExplosionEvent);
		TestTrue(TEXT("Explosion event forces urgent simulation scheduling"), HasActiveDynamicSimulationEvent(DynamicEvents));
		TestTrue(TEXT("Explosion event routes to vortex event buffer"), IsVortexPipelineEvent(UrgentExplosionEvent));
		TestEqual(
			TEXT("Sparse atlas scatter maps one 4x4x4 compute group per minimum brick"),
			ComputeSparseScatterGroupsPerBrick(TimeThiefSmokeParameterDefaults::SmokeBrickMinSize),
			1u);
		TestEqual(
			TEXT("Sparse atlas scatter dispatches eight groups for an 8-cell brick"),
			ComputeSparseScatterGroupsPerBrick(TimeThiefSmokeParameterDefaults::SmokeThreadGroupSize * 2),
			8u);
		TestTrue(
			TEXT("Sparse composite uses the sparse atlas when active ratio is low"),
			ShouldUseSparseComposite(FIntVector(4, 4, 4), 8u, 64u));
		TestFalse(
			TEXT("Sparse composite skips when active count is empty"),
			ShouldUseSparseComposite(FIntVector(4, 4, 4), 0u, 64u));
		TestFalse(
			TEXT("Sparse composite respects zero atlas capacity"),
			ShouldUseSparseComposite(FIntVector(16, 16, 16), 8u, 0u));
		TestFalse(
			TEXT("Sparse composite falls back to dense when active ratio is high"),
			ShouldUseSparseComposite(FIntVector(4, 4, 4), 60u, 64u));
		TestFalse(
			TEXT("Source-only simulation does not force vortex before the substep interval"),
			ShouldRunVortexPasses(
				false,
				false,
				0,
				TimeThiefSmokeParameterDefaults::VortexSubstepIntervalSeconds * 0.5f));
		TestTrue(
			TEXT("Vortex events run vortex immediately"),
			ShouldRunVortexPasses(
				false,
				false,
				1,
				0.0f));
		TestTrue(
			TEXT("Idle vortex runs on its substep interval"),
			ShouldRunVortexPasses(
				false,
				false,
				0,
				TimeThiefSmokeParameterDefaults::VortexSubstepIntervalSeconds));
		TestFalse(
			TEXT("Empty idle sparse smoke is removed from render candidates"),
			ShouldIncludeSmokeRenderCandidate(
				ETimeThiefSmokeSimulationBackend::SparseMac,
				false,
				0u,
				false));
		TestTrue(
			TEXT("Sparse smoke with renderable sparse state stays renderable"),
			ShouldIncludeSmokeRenderCandidate(
				ETimeThiefSmokeSimulationBackend::SparseMac,
				false,
				8u,
				false));
		TestTrue(
			TEXT("Sparse smoke with pending active count readback stays renderable"),
			ShouldIncludeSmokeRenderCandidate(
				ETimeThiefSmokeSimulationBackend::SparseMac,
				false,
				0u,
				true));
		TestTrue(
			TEXT("Dense smoke is always renderable through the legacy path"),
			ShouldIncludeSmokeRenderCandidate(
				ETimeThiefSmokeSimulationBackend::DenseLegacy,
				false,
				0u,
				false));
		FTimeThiefSmokeRendererVolume LifetimeVolume;
		LifetimeVolume.AgeSeconds = LifetimeVolume.DurationSeconds + TimeThiefSmokeParameterDefaults::SmokeFadeOutDuration + 0.1f;
		TestTrue(TEXT("Sparse smoke lifetime gate closes after fade-out"), IsPastSparseRenderableLifetime(LifetimeVolume));

		TArray<FTimeThiefSmokeRendererEvent> SortedEvents;
		FTimeThiefSmokeRendererEvent LowPriorityEvent;
		LowPriorityEvent.Type = ETimeThiefSmokeRendererInteractionType::BulletWake;
		LowPriorityEvent.Strength = 1.0f;
		LowPriorityEvent.Radius = 10.0f;
		LowPriorityEvent.NormalizedAge = 0.9f;
		SortedEvents.Add(LowPriorityEvent);

		FTimeThiefSmokeRendererEvent HighPriorityEvent;
		HighPriorityEvent.Type = ETimeThiefSmokeRendererInteractionType::ExplosionShock;
		HighPriorityEvent.Strength = 10.0f;
		HighPriorityEvent.Radius = 100.0f;
		HighPriorityEvent.NormalizedAge = 0.0f;
		SortedEvents.Add(HighPriorityEvent);

		SortAndClampSmokeEvents(SortedEvents, 1);
		TestEqual(TEXT("Global event clamp keeps highest priority event"), SortedEvents.Num(), 1);
		TestEqual(TEXT("Explosion priority survives small global event clamp"), SortedEvents[0].Type, ETimeThiefSmokeRendererInteractionType::ExplosionShock);

		TArray<FTimeThiefSmokeRendererEvent> TypedEvents;
		FTimeThiefSmokeRendererEvent ExplosionEvent = HighPriorityEvent;
		ExplosionEvent.Strength = 100.0f;
		TypedEvents.Add(ExplosionEvent);

		for (int32 EventIndex = 0; EventIndex < TimeThiefSmokeParameterDefaults::MaxSimulationBulletEventCount + 8; ++EventIndex)
		{
			FTimeThiefSmokeRendererEvent BulletEvent;
			BulletEvent.Type = ETimeThiefSmokeRendererInteractionType::BulletWake;
			BulletEvent.Strength = 10.0f + static_cast<float>(EventIndex);
			BulletEvent.Radius = 32.0f;
			BulletEvent.NormalizedAge = 0.0f;
			TypedEvents.Add(BulletEvent);
		}

		SortAndClampSmokeEvents(TypedEvents, TimeThiefSmokeParameterDefaults::MaxShaderEventCount);
		ClampSmokeEventsByType(TypedEvents, ETimeThiefSmokeRendererInteractionType::BulletWake, TimeThiefSmokeParameterDefaults::MaxSimulationBulletEventCount);

		int32 BulletEventCount = 0;
		bool bHasExplosionEvent = false;
		for (const FTimeThiefSmokeRendererEvent& Event : TypedEvents)
		{
			if (Event.Type == ETimeThiefSmokeRendererInteractionType::BulletWake)
			{
				++BulletEventCount;
			}
			else if (Event.Type == ETimeThiefSmokeRendererInteractionType::ExplosionShock)
			{
				bHasExplosionEvent = true;
			}
		}

		TestEqual(TEXT("Bullet simulation events are clamped by type"), BulletEventCount, TimeThiefSmokeParameterDefaults::MaxSimulationBulletEventCount);
		TestTrue(TEXT("Type clamp preserves non-bullet events"), bHasExplosionEvent);

		return true;
	}
#endif
}

FTimeThiefSmokeViewExtension::FTimeThiefSmokeViewExtension(const FAutoRegister& AutoRegister)
	: FSceneViewExtensionBase(AutoRegister)
{
}

void FTimeThiefSmokeViewExtension::ConsumeSparseActiveBrickCountReadback(FRenderSmokeState& State)
{
	if (!State.SparseActiveBrickCountReadback.IsValid() || !State.SparseActiveBrickCountReadback->IsReady())
	{
		return;
	}

	const uint32* Buffer = static_cast<const uint32*>(State.SparseActiveBrickCountReadback->Lock(sizeof(uint32)));
	State.SparseActiveBrickCount = Buffer ? *Buffer : 0u;
	State.SparseActiveBrickCountReadback->Unlock();
	State.SparseActiveBrickCountReadback.Reset();
	State.bSparseActiveBrickCountReadbackPending = false;
}

void FTimeThiefSmokeViewExtension::QueueSparseActiveBrickCountReadback(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGBufferRef ActiveBrickCountBuffer)
{
	if (!ActiveBrickCountBuffer || State.bSparseActiveBrickCountReadbackPending)
	{
		return;
	}

	State.SparseActiveBrickCountReadback = MakeUnique<FRHIGPUBufferReadback>(TEXT("TimeThiefSmoke.SparseActiveBrickCountReadback"));
	AddEnqueueCopyPass(GraphBuilder, State.SparseActiveBrickCountReadback.Get(), ActiveBrickCountBuffer, sizeof(uint32));
	State.bSparseActiveBrickCountReadbackPending = true;
}

bool FTimeThiefSmokeViewExtension::HasRenderableSceneState_RenderThread(uint64 SceneKey, const FSceneViewFamily* ViewFamily) const
{
	for (const TPair<FRenderSmokeStateKey, FRenderSmokeState>& Pair : SmokeStates)
	{
		if (Pair.Key.SceneKey != SceneKey)
		{
			continue;
		}

		const FRenderSmokeState& State = Pair.Value;
		if (ViewFamily && !IsSmokeVisibleInViewFamily(*ViewFamily, State.Volume))
		{
			continue;
		}

		if (ShouldIncludeSmokeRenderCandidate(
			GetDefaultSimulationBackend(),
			State.bNeedsInit,
			State.SparseActiveBrickCount,
			State.bSparseActiveBrickCountReadbackPending))
		{
			return true;
		}
	}

	return false;
}

void FTimeThiefSmokeViewExtension::SubmitFrame_RenderThread(FTimeThiefSmokeRendererFrame&& Frame)
{
	check(IsInRenderingThread());

	if (Frame.SceneKey == 0)
	{
		return;
	}

	LastFrameDeltaSecondsByScene.FindOrAdd(Frame.SceneKey) = FMath::Clamp(Frame.DeltaSeconds, 0.0f, 1.0f / 15.0f);

	TSet<FRenderSmokeStateKey> ActiveSmokeKeys;
	ActiveSmokeKeys.Reserve(Frame.Volumes.Num());
	for (FTimeThiefSmokeRendererVolume& Volume : Frame.Volumes)
	{
		if (Volume.SmokeId == INDEX_NONE)
		{
			continue;
		}

		const FRenderSmokeStateKey StateKey{ Frame.SceneKey, Volume.SmokeId };
		ActiveSmokeKeys.Add(StateKey);
		FRenderSmokeState& State = SmokeStates.FindOrAdd(StateKey);
		const bool bSparseBackend = GetDefaultSimulationBackend() == ETimeThiefSmokeSimulationBackend::SparseMac;
		if (!bSparseBackend)
		{
			State.bSparseAtlasScatterPending = false;
			State.bSparseOccupancyRefreshPending = false;
			State.SparseActiveBrickCount = 0u;
			State.bSparseActiveBrickCountReadbackPending = false;
			State.SparseActiveBrickCountReadback.Reset();
		}
		const FIntVector RequiredGridSize = MakeStableGridSize(Volume);
		if (!GridContains(State.AllocatedGridSize, RequiredGridSize))
		{
			State.AllocatedGridSize = MakeAllocatedGridSize(RequiredGridSize);
			State.DensityTextures[0].SafeRelease();
			State.DensityTextures[1].SafeRelease();
			State.DisplacedDensityTextures[0].SafeRelease();
			State.DisplacedDensityTextures[1].SafeRelease();
			State.VelocityTextures[0].SafeRelease();
			State.VelocityTextures[1].SafeRelease();
			State.BulletCutoutTextures[0].SafeRelease();
			State.BulletCutoutTextures[1].SafeRelease();
			State.BulletSinkTextures[0].SafeRelease();
			State.BulletSinkTextures[1].SafeRelease();
			State.ObstacleSdfTexture.SafeRelease();
			State.ObstacleVelocityTexture.SafeRelease();
			State.ObstacleFaceOpenTexture.SafeRelease();
			State.BrickOccupancyTexture.SafeRelease();
			State.SparseFieldAtlasTexture.SafeRelease();
			State.VortexParticleBuffers[0].SafeRelease();
			State.VortexParticleBuffers[1].SafeRelease();
			State.CurrentDensityIndex = 0;
			State.CurrentVelocityIndex = 0;
			State.CurrentBulletFieldIndex = 0;
			State.CurrentVortexParticleIndex = 0;
			State.AllocatedObstacleGridSize = FIntVector::ZeroValue;
			State.AllocatedBrickGridSize = FIntVector::ZeroValue;
			State.AllocatedSparseAtlasBrickGridSize = FIntVector::ZeroValue;
			State.AllocatedSparseAtlasGridSize = FIntVector::ZeroValue;
			State.AllocatedSparseAtlasBrickCapacity = 0;
			State.UploadedObstacleFieldRevision = MAX_uint32;
			State.LastSimulatedFrame = MAX_uint32;
			State.SparseActiveBrickCount = 0u;
			State.bSparseActiveBrickCountReadbackPending = false;
			State.SparseActiveBrickCountReadback.Reset();
			State.AllocatedVortexParticleCount = 0;
			State.AccumulatedSimulationDeltaSeconds = 0.0f;
			State.AccumulatedVortexDeltaSeconds = 0.0f;
			State.BulletFieldDecayBudgetSeconds = 0.0f;
			State.bVortexParticlesNeedUpload = true;
			State.bSparseAtlasScatterPending = false;
			State.bSparseOccupancyRefreshPending = false;
			State.bUseSparseSimulationMaskThisFrame = false;
			State.bNeedsInit = true;
		}
		if (Volume.ObstaclePrimitives.IsEmpty() &&
			Volume.bHasSolidObstacleField &&
			Volume.ObstacleFieldRevision == State.Volume.ObstacleFieldRevision &&
			!State.Volume.ObstaclePrimitives.IsEmpty())
		{
			Volume.ObstaclePrimitives = MoveTemp(State.Volume.ObstaclePrimitives);
		}
		State.Volume = MoveTemp(Volume);
		if (bSparseBackend && IsPastSparseRenderableLifetime(State.Volume))
		{
			State.SparseActiveBrickCount = 0u;
			State.bSparseActiveBrickCountReadbackPending = false;
			State.SparseActiveBrickCountReadback.Reset();
			State.bSparseAtlasScatterPending = false;
			State.bSparseOccupancyRefreshPending = false;
		}
	}

	for (auto It = SmokeStates.CreateIterator(); It; ++It)
	{
		if (It.Key().SceneKey == Frame.SceneKey && !ActiveSmokeKeys.Contains(It.Key()))
		{
			It.RemoveCurrent();
		}
	}

	for (const FTimeThiefSmokeRendererEvent& Event : Frame.Events)
	{
		if (FRenderSmokeState* State = SmokeStates.Find(FRenderSmokeStateKey{ Frame.SceneKey, Event.SmokeId }))
		{
			const int32 MaxEvents = FMath::Clamp(TimeThiefSmokeParameterDefaults::MaxGPUEventsPerSmokePerFrame, 0, TimeThiefSmokeParameterDefaults::MaxShaderEventCount);
			AddPrioritizedSmokeEvent(State->PendingEvents, Event, MaxEvents);
		}
	}

	if (ActiveSmokeKeys.IsEmpty())
	{
		LastFrameDeltaSecondsByScene.Remove(Frame.SceneKey);
	}
}

void FTimeThiefSmokeViewExtension::Clear_RenderThread()
{
	check(IsInRenderingThread());
	SmokeStates.Reset();
	LastFrameDeltaSecondsByScene.Reset();
}

void FTimeThiefSmokeViewExtension::PreRenderViewFamily_RenderThread(
	FRDGBuilder& GraphBuilder,
	FSceneViewFamily& ViewFamily)
{
	const uint64 SceneKey = GetSceneKey(ViewFamily);
	const float FrameDeltaSeconds = FMath::Clamp(LastFrameDeltaSecondsByScene.FindRef(SceneKey), 0.0f, 0.1f);
	for (TPair<FRenderSmokeStateKey, FRenderSmokeState>& SmokePair : SmokeStates)
	{
		if (SmokePair.Key.SceneKey != SceneKey)
		{
			continue;
		}

		FRenderSmokeState& State = SmokePair.Value;
		ConsumeSparseActiveBrickCountReadback(State);
		if (State.LastSimulatedFrame == GFrameNumberRenderThread)
		{
			continue;
		}

		const float SimulationHz = CVarTimeThiefSmokeSimulationHz.GetValueOnRenderThread();
		const float SimulationInterval = SimulationHz > 0.0f ? 1.0f / SimulationHz : 0.0f;
		const bool bSparseBackend = GetDefaultSimulationBackend() == ETimeThiefSmokeSimulationBackend::SparseMac;
		const bool bHasActiveDynamicEvents = HasActiveDynamicSimulationEvent(State.PendingEvents);
		State.bSparseAtlasVisibleThisFrame = !bSparseBackend || IsSmokeVisibleInViewFamily(ViewFamily, State.Volume);
		const bool bPastSparseRenderableLifetime = bSparseBackend && IsPastSparseRenderableLifetime(State.Volume);
		const bool bForceSparseAtlasRefresh = bSparseBackend && State.bSparseAtlasVisibleThisFrame && State.bSparseAtlasScatterPending;
		const bool bHasActiveEvents = bHasActiveDynamicEvents || bForceSparseAtlasRefresh;
		if (bPastSparseRenderableLifetime &&
			!bHasActiveDynamicEvents &&
			State.BulletFieldDecayBudgetSeconds <= UE_SMALL_NUMBER)
		{
			State.SparseActiveBrickCount = 0u;
			State.bSparseActiveBrickCountReadbackPending = false;
			State.SparseActiveBrickCountReadback.Reset();
			State.bSparseAtlasScatterPending = false;
			State.bSparseOccupancyRefreshPending = false;
			State.LastSimulatedFrame = GFrameNumberRenderThread;
			continue;
		}
		const float EffectiveSimulationInterval = SimulationInterval;
		State.AccumulatedSimulationDeltaSeconds = FMath::Min(State.AccumulatedSimulationDeltaSeconds + FrameDeltaSeconds, 0.1f);
		if (!bForceSparseAtlasRefresh && !State.bNeedsInit && EffectiveSimulationInterval > 0.0f && State.AccumulatedSimulationDeltaSeconds < EffectiveSimulationInterval)
		{
			State.LastSimulatedFrame = GFrameNumberRenderThread;
			continue;
		}
		const float SimulationDeltaSeconds = EffectiveSimulationInterval > 0.0f
			? FMath::Max(State.AccumulatedSimulationDeltaSeconds, FrameDeltaSeconds)
			: FrameDeltaSeconds;
		State.AccumulatedSimulationDeltaSeconds = 0.0f;
		if (!State.bNeedsInit &&
			bSparseBackend &&
			!HasRenderableSparseState(State.SparseActiveBrickCount, State.bSparseActiveBrickCountReadbackPending) &&
			!bHasActiveEvents)
		{
			State.LastSimulatedFrame = GFrameNumberRenderThread;
			continue;
		}

		SimulateSmoke(GraphBuilder, State, SimulationDeltaSeconds);
		State.LastSimulatedFrame = GFrameNumberRenderThread;
	}
}

void FTimeThiefSmokeViewExtension::SubscribeToPostProcessingPass(
	EPostProcessingPass Pass,
	const FSceneView& View,
	FAfterPassCallbackDelegateArray& InOutPassCallbacks,
	bool bIsPassEnabled)
{
	if (Pass == EPostProcessingPass::Tonemap && bIsPassEnabled && HasRenderableSceneState_RenderThread(GetSceneKey(View), View.Family))
	{
		InOutPassCallbacks.Add(FAfterPassCallbackDelegate::CreateRaw(this, &FTimeThiefSmokeViewExtension::CompositeSmoke_RenderThread));
	}
}

FScreenPassTexture FTimeThiefSmokeViewExtension::CompositeSmokeMulti_RenderThread(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const FPostProcessMaterialInputs& Inputs,
	const TArray<FRenderSmokeState*>& RenderStates,
	const TArray<FIntRect>& RenderRects,
	FScreenPassTexture CurrentSceneColor,
	const FMatrix44f& InvViewProjection,
	bool bAllowOverrideOutput)
{
	if (!CurrentSceneColor.IsValid() ||
		RenderStates.IsEmpty() ||
		RenderStates.Num() > TimeThiefSmokeParameterDefaults::MaxCompositeSmokeSlots ||
		RenderRects.Num() != RenderStates.Num())
	{
		return CurrentSceneColor;
	}

	const bool bUseOverrideOutput = bAllowOverrideOutput &&
		Inputs.OverrideOutput.IsValid() &&
		Inputs.OverrideOutput.Texture != CurrentSceneColor.Texture;
	const FScreenPassViewInfo ViewInfo(View);
	const FIntRect ViewRect = bUseOverrideOutput ? Inputs.OverrideOutput.ViewRect : CurrentSceneColor.ViewRect;
	const int32 SmokeCount = RenderStates.Num();

	FIntRect CompositeRect = RenderRects[0];
	for (int32 SmokeIndex = 1; SmokeIndex < SmokeCount; ++SmokeIndex)
	{
		const FIntRect& SmokeRect = RenderRects[SmokeIndex];
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
		FMath::Max(1, FMath::DivideAndRoundUp(DrawRect.Width(), TimeThiefSmokeParameterDefaults::CompositeTileSize)),
		FMath::Max(1, FMath::DivideAndRoundUp(DrawRect.Height(), TimeThiefSmokeParameterDefaults::CompositeTileSize)));
	const int32 TileCount = TileGridSize.X * TileGridSize.Y;

	TArray<int32> TileSmokeCounts;
	TileSmokeCounts.Init(0, TileCount);
	TArray<FIntRect, TInlineAllocator<TimeThiefSmokeParameterDefaults::MaxCompositeSmokeSlots>> SmokeTileRects;
	SmokeTileRects.SetNum(SmokeCount);
	for (int32 SmokeSlot = 0; SmokeSlot < SmokeCount; ++SmokeSlot)
	{
		const FIntRect& SmokeRect = RenderRects[SmokeSlot];
		const int32 MinTileX = FMath::Clamp((SmokeRect.Min.X - DrawRect.Min.X) / TimeThiefSmokeParameterDefaults::CompositeTileSize, 0, TileGridSize.X - 1);
		const int32 MinTileY = FMath::Clamp((SmokeRect.Min.Y - DrawRect.Min.Y) / TimeThiefSmokeParameterDefaults::CompositeTileSize, 0, TileGridSize.Y - 1);
		const int32 MaxTileX = FMath::Clamp((FMath::Max(SmokeRect.Max.X - 1, SmokeRect.Min.X) - DrawRect.Min.X) / TimeThiefSmokeParameterDefaults::CompositeTileSize, 0, TileGridSize.X - 1);
		const int32 MaxTileY = FMath::Clamp((FMath::Max(SmokeRect.Max.Y - 1, SmokeRect.Min.Y) - DrawRect.Min.Y) / TimeThiefSmokeParameterDefaults::CompositeTileSize, 0, TileGridSize.Y - 1);
		SmokeTileRects[SmokeSlot] = FIntRect(MinTileX, MinTileY, MaxTileX + 1, MaxTileY + 1);
		for (int32 TileY = SmokeTileRects[SmokeSlot].Min.Y; TileY < SmokeTileRects[SmokeSlot].Max.Y; ++TileY)
		{
			for (int32 TileX = SmokeTileRects[SmokeSlot].Min.X; TileX < SmokeTileRects[SmokeSlot].Max.X; ++TileX)
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
	for (int32 TileIndex = 0; TileIndex < TileCount; ++TileIndex)
	{
		const int32 Count = TileSmokeCounts[TileIndex];
		TileWriteOffsets[TileIndex] = TotalTileSmokeIndexCount;
		TileRanges[TileIndex].OffsetCount = FVector2f(static_cast<float>(TotalTileSmokeIndexCount), static_cast<float>(Count));
		TotalTileSmokeIndexCount += Count;
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
		for (int32 SmokeSlot = 0; SmokeSlot < SmokeCount; ++SmokeSlot)
		{
			const FIntRect& SmokeTileRect = SmokeTileRects[SmokeSlot];
			for (int32 TileY = SmokeTileRect.Min.Y; TileY < SmokeTileRect.Max.Y; ++TileY)
			{
				for (int32 TileX = SmokeTileRect.Min.X; TileX < SmokeTileRect.Max.X; ++TileX)
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

	TArray<FTimeThiefSmokeCompositeDescriptorShaderData, TInlineAllocator<TimeThiefSmokeParameterDefaults::MaxCompositeSmokeSlots>> Descriptors;
	Descriptors.Reserve(SmokeCount);
	const bool bUseFastFilament = CVarTimeThiefSmokeFastFilament.GetValueOnRenderThread() != 0;

	for (int32 SmokeSlot = 0; SmokeSlot < SmokeCount; ++SmokeSlot)
	{
		const FRenderSmokeState& State = *RenderStates[SmokeSlot];
		const int32 SelfShadowStepCount = FMath::Max(TimeThiefSmokeParameterDefaults::SelfShadowStepCount, 0);
		const int32 RenderStepCount = FMath::Clamp(
			TimeThiefSmokeParameterDefaults::RenderStepCount,
			TimeThiefSmokeParameterDefaults::RenderStepCountMin,
			TimeThiefSmokeParameterDefaults::RenderStepCountMax);
		const int32 RenderMaxStepCount = FMath::Clamp(
			TimeThiefSmokeParameterDefaults::RenderMaxStepCount,
			TimeThiefSmokeParameterDefaults::RenderMaxStepCountMin,
			TimeThiefSmokeParameterDefaults::RenderMaxStepCountMax);
		const FMatrix44f WorldToLocal = State.Volume.LocalToWorld.ToInverseMatrixWithScale();
		const int32 SparseAtlasBrickCapacity = FMath::Max(State.AllocatedSparseAtlasBrickCapacity, 1);
		const bool bUseSparseComposite = GetDefaultSimulationBackend() == ETimeThiefSmokeSimulationBackend::SparseMac &&
			ShouldUseSparseComposite(State.AllocatedBrickGridSize, State.SparseActiveBrickCount, static_cast<uint32>(SparseAtlasBrickCapacity));

		FTimeThiefSmokeCompositeDescriptorShaderData Descriptor;
		Descriptor.WorldToLocal0 = MakeMatrixRow(WorldToLocal, 0);
		Descriptor.WorldToLocal1 = MakeMatrixRow(WorldToLocal, 1);
		Descriptor.WorldToLocal2 = MakeMatrixRow(WorldToLocal, 2);
		Descriptor.WorldToLocal3 = MakeMatrixRow(WorldToLocal, 3);
		const FVector3f SelfShadowLightDirection = TimeThiefSmokeParameterDefaults::GetSelfShadowLightDirection();
		Descriptor.BoundsExtent_RenderStepVoxelScale = FVector4f(State.Volume.BoundsExtent.X, State.Volume.BoundsExtent.Y, State.Volume.BoundsExtent.Z, FMath::Clamp(TimeThiefSmokeParameterDefaults::RenderStepVoxelScale, TimeThiefSmokeParameterDefaults::RenderStepVoxelScaleMin, TimeThiefSmokeParameterDefaults::RenderStepVoxelScaleMax));
		Descriptor.RenderBoundsExtent_Extinction = FVector4f(State.Volume.RenderBoundsExtent.X, State.Volume.RenderBoundsExtent.Y, State.Volume.RenderBoundsExtent.Z, FMath::Max(0.0f, TimeThiefSmokeParameterDefaults::Extinction));
		Descriptor.ScatterNoise = FVector4f(FMath::Clamp(TimeThiefSmokeParameterDefaults::ScatteringAlbedo, 0.0f, 1.0f), FMath::Clamp(TimeThiefSmokeParameterDefaults::ScatteringAnisotropy, -1.0f, 1.0f), FMath::Max(0.0f, TimeThiefSmokeParameterDefaults::RenderNoiseScale), FMath::Max(0.0f, TimeThiefSmokeParameterDefaults::RenderNoiseStrength));
		Descriptor.SelfShadowLightDirection_StepCount = FVector4f(SelfShadowLightDirection.X, SelfShadowLightDirection.Y, SelfShadowLightDirection.Z, static_cast<float>(SelfShadowStepCount));
		Descriptor.SelfShadowControls = FVector4f(FMath::Max(0.0f, TimeThiefSmokeParameterDefaults::SelfShadowStrength), FMath::Max(0.0f, TimeThiefSmokeParameterDefaults::SelfShadowExtinction), FMath::Max(0.0f, TimeThiefSmokeParameterDefaults::SelfShadowStepLength), static_cast<float>(TimeThiefSmokeParameterDefaults::SelfShadowInactiveBrickMaxSkipSteps));
		Descriptor.NoiseFilamentA = FVector4f(FMath::Max(0.0f, TimeThiefSmokeParameterDefaults::RenderNoiseTimeScale), FMath::Max(0.0f, TimeThiefSmokeParameterDefaults::RenderFilamentScale), FMath::Max(0.0f, TimeThiefSmokeParameterDefaults::RenderFilamentStrength), FMath::Max(0.0f, TimeThiefSmokeParameterDefaults::RenderFilamentContrast));
		Descriptor.FilamentAge = FVector4f(FMath::Max(0.0f, TimeThiefSmokeParameterDefaults::RenderFilamentWarpStrength), State.Volume.AgeSeconds, State.Volume.DurationSeconds, FMath::Max(0.0f, TimeThiefSmokeParameterDefaults::SmokeFadeOutDuration));
		Descriptor.GridResolution_UseSparse = FVector4f(static_cast<float>(State.AllocatedGridSize.X), static_cast<float>(State.AllocatedGridSize.Y), static_cast<float>(State.AllocatedGridSize.Z), bUseSparseComposite ? 1.0f : 0.0f);
		Descriptor.BrickGridResolution_SmokeBrickSize = FVector4f(static_cast<float>(State.AllocatedBrickGridSize.X), static_cast<float>(State.AllocatedBrickGridSize.Y), static_cast<float>(State.AllocatedBrickGridSize.Z), static_cast<float>(FMath::Clamp(TimeThiefSmokeParameterDefaults::SmokeBrickSize, TimeThiefSmokeParameterDefaults::SmokeBrickMinSize, TimeThiefSmokeParameterDefaults::SmokeBrickMaxSize)));
		Descriptor.SparseAtlasBrickGridResolution_MaxActive = FVector4f(static_cast<float>(State.AllocatedSparseAtlasBrickGridSize.X), static_cast<float>(State.AllocatedSparseAtlasBrickGridSize.Y), static_cast<float>(State.AllocatedSparseAtlasBrickGridSize.Z), static_cast<float>(SparseAtlasBrickCapacity));
		Descriptor.RenderSteps_Quality = FVector4f(static_cast<float>(RenderStepCount), static_cast<float>(RenderMaxStepCount), 0.0f, bUseFastFilament ? 1.0f : 0.0f);
		Descriptor.NaturalBoundsExtent_ObstacleFeather = FVector4f(State.Volume.NaturalBoundsExtent.X, State.Volume.NaturalBoundsExtent.Y, State.Volume.NaturalBoundsExtent.Z, TimeThiefSmokeParameterDefaults::ObstacleSdfSurfaceFeatherCm);
		Descriptor.BoundaryNoiseControls = FVector4f(
			FMath::Max(0.0f, TimeThiefSmokeParameterDefaults::RenderBoundaryNoiseScale),
			FMath::Max(0.0f, TimeThiefSmokeParameterDefaults::RenderBoundaryNoiseStrength),
			0.0f,
			0.0f);
		const bool bHasBulletFields =
			State.BulletCutoutTextures[State.CurrentBulletFieldIndex].IsValid() &&
			State.BulletSinkTextures[State.CurrentBulletFieldIndex].IsValid();
		Descriptor.RaymarchControls = FVector4f(TimeThiefSmokeParameterDefaults::InactiveBrickRaymarchMaxSkipScale, TimeThiefSmokeParameterDefaults::RenderTransmittanceEarlyOut, bHasBulletFields ? 1.0f : 0.0f, TimeThiefSmokeParameterDefaults::SelfShadowMinSampleWeight);
		Descriptors.Add(Descriptor);
	}

	FRDGBufferRef DescriptorBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(FTimeThiefSmokeCompositeDescriptorShaderData), Descriptors.Num()), TEXT("TimeThiefSmoke.CompositeMultiDescriptors"));
	GraphBuilder.QueueBufferUpload(DescriptorBuffer, Descriptors.GetData(), Descriptors.Num() * sizeof(FTimeThiefSmokeCompositeDescriptorShaderData));
	FRDGBufferRef TileRangeBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(FTimeThiefSmokeCompositeTileRangeShaderData), TileRanges.Num()), TEXT("TimeThiefSmoke.CompositeMultiTileRanges"));
	GraphBuilder.QueueBufferUpload(TileRangeBuffer, TileRanges.GetData(), TileRanges.Num() * sizeof(FTimeThiefSmokeCompositeTileRangeShaderData));
	FRDGBufferRef TileIndexBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), TileIndices.Num()), TEXT("TimeThiefSmoke.CompositeMultiTileIndices"));
	GraphBuilder.QueueBufferUpload(TileIndexBuffer, TileIndices.GetData(), TileIndices.Num() * sizeof(uint32));

	TShaderMapRef<FTimeThiefSmokeCompositeMultiPS> PixelShader(GetGlobalShaderMap(View.FeatureLevel));
	FTimeThiefSmokeCompositeMultiPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeCompositeMultiPS::FParameters>();
	PassParameters->SceneColorTexture = CurrentSceneColor.Texture;
	PassParameters->SceneDepthTexture = Inputs.SceneTextures.SceneTextures->GetParameters()->SceneDepthTexture;
	PassParameters->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->VolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->CompositeSmokeDescriptors = GraphBuilder.CreateSRV(DescriptorBuffer);
	PassParameters->TileSmokeRanges = GraphBuilder.CreateSRV(TileRangeBuffer);
	PassParameters->TileSmokeIndices = GraphBuilder.CreateSRV(TileIndexBuffer);

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
	PassParameters->CompositeTileSize = TimeThiefSmokeParameterDefaults::CompositeTileSize;
	PassParameters->SmokeSlotCount = SmokeCount;
	PassParameters->InvViewProjection = InvViewProjection;
	struct FMultiSmokeTextureRefs
	{
		FRDGTextureRef DensityTexture = nullptr;
		FRDGTextureRef DisplacedDensityTexture = nullptr;
		FRDGTextureRef ObstacleTexture = nullptr;
		FRDGTextureRef BulletCutoutTexture = nullptr;
		FRDGTextureRef BulletSinkTexture = nullptr;
		FRDGTextureRef BrickOccupancyTexture = nullptr;
		FRDGTextureRef SparseFieldAtlasTexture = nullptr;
	};

	TArray<FMultiSmokeTextureRefs, TInlineAllocator<TimeThiefSmokeParameterDefaults::MaxCompositeSmokeSlots>> MultiSmokeTextures;
	MultiSmokeTextures.SetNum(SmokeCount);
	for (int32 SmokeSlot = 0; SmokeSlot < SmokeCount; ++SmokeSlot)
	{
		FRenderSmokeState& State = *RenderStates[SmokeSlot];
		FMultiSmokeTextureRefs& TextureRefs = MultiSmokeTextures[SmokeSlot];
		TextureRefs.DensityTexture = GraphBuilder.RegisterExternalTexture(State.DensityTextures[State.CurrentDensityIndex]);
		TextureRefs.DisplacedDensityTexture = GraphBuilder.RegisterExternalTexture(State.DisplacedDensityTextures[State.CurrentDensityIndex]);
		TextureRefs.ObstacleTexture = GraphBuilder.RegisterExternalTexture(State.ObstacleSdfTexture);
		TextureRefs.BulletCutoutTexture = State.BulletCutoutTextures[State.CurrentBulletFieldIndex].IsValid()
			? GraphBuilder.RegisterExternalTexture(State.BulletCutoutTextures[State.CurrentBulletFieldIndex])
			: TextureRefs.DensityTexture;
		TextureRefs.BulletSinkTexture = State.BulletSinkTextures[State.CurrentBulletFieldIndex].IsValid()
			? GraphBuilder.RegisterExternalTexture(State.BulletSinkTextures[State.CurrentBulletFieldIndex])
			: TextureRefs.DensityTexture;
		TextureRefs.BrickOccupancyTexture = GraphBuilder.RegisterExternalTexture(State.BrickOccupancyTexture);
		TextureRefs.SparseFieldAtlasTexture = GraphBuilder.RegisterExternalTexture(State.SparseFieldAtlasTexture);
	}

	FRDGTextureRef* DensityTargets[] = { &PassParameters->DensityTexture0, &PassParameters->DensityTexture1, &PassParameters->DensityTexture2, &PassParameters->DensityTexture3, &PassParameters->DensityTexture4, &PassParameters->DensityTexture5, &PassParameters->DensityTexture6 };
	FRDGTextureRef* DisplacedDensityTargets[] = { &PassParameters->DisplacedDensityTexture0, &PassParameters->DisplacedDensityTexture1, &PassParameters->DisplacedDensityTexture2, &PassParameters->DisplacedDensityTexture3, &PassParameters->DisplacedDensityTexture4, &PassParameters->DisplacedDensityTexture5, &PassParameters->DisplacedDensityTexture6 };
	FRDGTextureRef* ObstacleTargets[] = { &PassParameters->ObstacleTexture0, &PassParameters->ObstacleTexture1, &PassParameters->ObstacleTexture2, &PassParameters->ObstacleTexture3, &PassParameters->ObstacleTexture4, &PassParameters->ObstacleTexture5, &PassParameters->ObstacleTexture6 };
	FRDGTextureRef* BulletCutoutTargets[] = { &PassParameters->BulletCutoutTexture0, &PassParameters->BulletCutoutTexture1, &PassParameters->BulletCutoutTexture2, &PassParameters->BulletCutoutTexture3, &PassParameters->BulletCutoutTexture4, &PassParameters->BulletCutoutTexture5, &PassParameters->BulletCutoutTexture6 };
	FRDGTextureRef* BulletSinkTargets[] = { &PassParameters->BulletSinkTexture0, &PassParameters->BulletSinkTexture1, &PassParameters->BulletSinkTexture2, &PassParameters->BulletSinkTexture3, &PassParameters->BulletSinkTexture4, &PassParameters->BulletSinkTexture5, &PassParameters->BulletSinkTexture6 };
	FRDGTextureRef* BrickOccupancyTargets[] = { &PassParameters->BrickOccupancyTexture0, &PassParameters->BrickOccupancyTexture1, &PassParameters->BrickOccupancyTexture2, &PassParameters->BrickOccupancyTexture3, &PassParameters->BrickOccupancyTexture4, &PassParameters->BrickOccupancyTexture5, &PassParameters->BrickOccupancyTexture6 };
	FRDGTextureRef* SparseFieldAtlasTargets[] = { &PassParameters->SparseFieldAtlasTexture0, &PassParameters->SparseFieldAtlasTexture1, &PassParameters->SparseFieldAtlasTexture2, &PassParameters->SparseFieldAtlasTexture3, &PassParameters->SparseFieldAtlasTexture4, &PassParameters->SparseFieldAtlasTexture5, &PassParameters->SparseFieldAtlasTexture6 };

	for (int32 Slot = 0; Slot < TimeThiefSmokeParameterDefaults::MaxCompositeSmokeSlots; ++Slot)
	{
		const FMultiSmokeTextureRefs& TextureRefs = MultiSmokeTextures[FMath::Min(Slot, MultiSmokeTextures.Num() - 1)];
		*DensityTargets[Slot] = TextureRefs.DensityTexture;
		*DisplacedDensityTargets[Slot] = TextureRefs.DisplacedDensityTexture;
		*ObstacleTargets[Slot] = TextureRefs.ObstacleTexture;
		*BulletCutoutTargets[Slot] = TextureRefs.BulletCutoutTexture;
		*BulletSinkTargets[Slot] = TextureRefs.BulletSinkTexture;
		*BrickOccupancyTargets[Slot] = TextureRefs.BrickOccupancyTexture;
		*SparseFieldAtlasTargets[Slot] = TextureRefs.SparseFieldAtlasTexture;
	}
	PassParameters->RenderTargets[0] = Output.GetRenderTargetBinding();

	AddDrawScreenPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.CompositeMulti Smokes=%d Tiles=%dx%d", SmokeCount, TileGridSize.X, TileGridSize.Y),
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
	const bool bUseScissor = CVarTimeThiefSmokeScissor.GetValueOnRenderThread() != 0;
	const uint64 SceneKey = GetSceneKey(View);

	TArray<FRenderSmokeState*> RenderStates;
	RenderStates.Reserve(SmokeStates.Num());
	for (TPair<FRenderSmokeStateKey, FRenderSmokeState>& SmokePair : SmokeStates)
	{
		if (SmokePair.Key.SceneKey != SceneKey)
		{
			continue;
		}

		FRenderSmokeState& State = SmokePair.Value;
		if (!ShouldIncludeSmokeRenderCandidate(
			GetDefaultSimulationBackend(),
			State.bNeedsInit,
			State.SparseActiveBrickCount,
			State.bSparseActiveBrickCountReadbackPending))
		{
			continue;
		}

		if (State.DensityTextures[State.CurrentDensityIndex].IsValid() &&
			State.DisplacedDensityTextures[State.CurrentDensityIndex].IsValid() &&
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

	struct FCompositeBatch
		{
			TArray<int32> CandidateIndices;
		};

		TArray<FCompositeBatch> Batches;

		TArray<int32> VisibleCandidateIndices;
		VisibleCandidateIndices.Reserve(Candidates.Num());
		FIntRect CombinedVisibleRect;
		bool bHasCombinedVisibleRect = false;
		bool bAnyFullscreenCandidate = false;
		for (int32 CandidateIndex = 0; CandidateIndex < Candidates.Num(); ++CandidateIndex)
		{
			if (!Candidates[CandidateIndex].bValid)
			{
				continue;
			}

			const FIntRect& CandidateRect = Candidates[CandidateIndex].Rect;
			VisibleCandidateIndices.Add(CandidateIndex);
			bAnyFullscreenCandidate |= ShouldUseFullscreenComposite(CandidateRect, CurrentSceneColor.ViewRect);
			if (!bHasCombinedVisibleRect)
			{
				CombinedVisibleRect = CandidateRect;
				bHasCombinedVisibleRect = true;
			}
			else
			{
				CombinedVisibleRect.Min.X = FMath::Min(CombinedVisibleRect.Min.X, CandidateRect.Min.X);
				CombinedVisibleRect.Min.Y = FMath::Min(CombinedVisibleRect.Min.Y, CandidateRect.Min.Y);
				CombinedVisibleRect.Max.X = FMath::Max(CombinedVisibleRect.Max.X, CandidateRect.Max.X);
				CombinedVisibleRect.Max.Y = FMath::Max(CombinedVisibleRect.Max.Y, CandidateRect.Max.Y);
			}
		}

		const bool bCanBatchAllVisibleSmokes =
			VisibleCandidateIndices.Num() <= TimeThiefSmokeParameterDefaults::MaxCompositeSmokeSlots &&
			(bAnyFullscreenCandidate || (bHasCombinedVisibleRect && ShouldUseFullscreenComposite(CombinedVisibleRect, CurrentSceneColor.ViewRect)));
		if (bCanBatchAllVisibleSmokes)
		{
			FCompositeBatch& Batch = Batches.AddDefaulted_GetRef();
			Batch.CandidateIndices = MoveTemp(VisibleCandidateIndices);
		}
		else
		{
			TArray<FIntRect> CandidateRects;
			TArray<uint8> CandidateValidFlags;
			CandidateRects.SetNum(Candidates.Num());
			CandidateValidFlags.SetNum(Candidates.Num());
			for (int32 CandidateIndex = 0; CandidateIndex < Candidates.Num(); ++CandidateIndex)
			{
				CandidateRects[CandidateIndex] = Candidates[CandidateIndex].Rect;
				CandidateValidFlags[CandidateIndex] = Candidates[CandidateIndex].bValid ? 1 : 0;
			}

			TMap<int32, TArray<int32>> GroupIndicesByRoot;
			BuildCompositeOverlapGroups(CandidateRects, CandidateValidFlags, CurrentSceneColor.ViewRect, GroupIndicesByRoot);

			TArray<TArray<int32>> SortedGroups;
			SortedGroups.Reserve(GroupIndicesByRoot.Num());
			for (TPair<int32, TArray<int32>>& GroupPair : GroupIndicesByRoot)
			{
				GroupPair.Value.Sort();
				SortedGroups.Add(GroupPair.Value);
			}
			SortedGroups.Sort([](const TArray<int32>& A, const TArray<int32>& B)
			{
				const int32 AFirst = A.IsEmpty() ? MAX_int32 : A[0];
				const int32 BFirst = B.IsEmpty() ? MAX_int32 : B[0];
				return AFirst < BFirst;
			});

			for (const TArray<int32>& GroupIndices : SortedGroups)
			{
				for (int32 GroupOffset = 0; GroupOffset < GroupIndices.Num(); GroupOffset += TimeThiefSmokeParameterDefaults::MaxCompositeSmokeSlots)
				{
					const int32 ChunkCount = FMath::Min(TimeThiefSmokeParameterDefaults::MaxCompositeSmokeSlots, GroupIndices.Num() - GroupOffset);
					if (ChunkCount <= 0)
					{
						continue;
					}

					FCompositeBatch& Batch = Batches.AddDefaulted_GetRef();
					Batch.CandidateIndices.Reserve(ChunkCount);
					for (int32 ChunkIndex = 0; ChunkIndex < ChunkCount; ++ChunkIndex)
					{
						Batch.CandidateIndices.Add(GroupIndices[GroupOffset + ChunkIndex]);
					}
				}
			}
		}

		for (int32 BatchIndex = 0; BatchIndex < Batches.Num(); ++BatchIndex)
		{
			const FCompositeBatch& Batch = Batches[BatchIndex];
			TArray<FRenderSmokeState*> GroupRenderStates;
			GroupRenderStates.Reserve(Batch.CandidateIndices.Num());
			TArray<FIntRect> GroupRects;
			GroupRects.Reserve(Batch.CandidateIndices.Num());
			for (const int32 CandidateIndex : Batch.CandidateIndices)
			{
				const FCompositeCandidate& Candidate = Candidates[CandidateIndex];
				GroupRenderStates.Add(Candidate.State);
				GroupRects.Add(Candidate.Rect);
			}

			const bool bLastBatch = BatchIndex == Batches.Num() - 1;
			const FScreenPassTexture MultiOutput = CompositeSmokeMulti_RenderThread(
				GraphBuilder,
				View,
				Inputs,
				GroupRenderStates,
				GroupRects,
				CurrentSceneColor,
				InvViewProjection,
				bLastBatch);
			if (MultiOutput.Texture == CurrentSceneColor.Texture)
			{
				continue;
			}

			CurrentSceneColor = MultiOutput;
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
	const bool bSparseBackend = GetDefaultSimulationBackend() == ETimeThiefSmokeSimulationBackend::SparseMac;
	const int32 BrickSize = FMath::Clamp(TimeThiefSmokeParameterDefaults::SmokeBrickSize, TimeThiefSmokeParameterDefaults::SmokeBrickMinSize, TimeThiefSmokeParameterDefaults::SmokeBrickMaxSize);
	const FIntVector BrickGridSize = bSparseBackend ? MakeBrickGridSize(GridSize, BrickSize) : FIntVector(1, 1, 1);
	const int32 SparseAtlasBrickCapacity = bSparseBackend
		? FMath::Clamp(TimeThiefSmokeParameterDefaults::MaxActiveSmokeBricks, 1, static_cast<int32>(GetBrickGridCount(BrickGridSize)))
		: 1;
	const FIntVector SparseAtlasBrickGridSize = bSparseBackend ? MakeSparseAtlasBrickGridSize(SparseAtlasBrickCapacity) : FIntVector(1, 1, 1);
	const FIntVector SparseAtlasGridSize = bSparseBackend ? MakeSparseAtlasGridSize(SparseAtlasBrickGridSize, BrickSize) : FIntVector(1, 1, 1);
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

		if (!State.DisplacedDensityTextures[TextureIndex].IsValid())
		{
			AllocatePooledTexture(DensityDesc, State.DisplacedDensityTextures[TextureIndex], TEXT("TimeThiefSmoke.DisplacedDensity"));
			State.bNeedsInit = true;
		}

		if (!State.VelocityTextures[TextureIndex].IsValid())
		{
			AllocatePooledTexture(VelocityDesc, State.VelocityTextures[TextureIndex], TEXT("TimeThiefSmoke.Velocity"));
			State.bNeedsInit = true;
		}
	}

	if (!State.BrickOccupancyTexture.IsValid() || State.AllocatedBrickGridSize != BrickGridSize)
	{
		State.BrickOccupancyTexture.SafeRelease();
		AllocatePooledTexture(BrickOccupancyDesc, State.BrickOccupancyTexture, TEXT("TimeThiefSmoke.BrickOccupancy"));
		State.AllocatedBrickGridSize = BrickGridSize;
	}

	if (!State.SparseFieldAtlasTexture.IsValid() ||
		State.AllocatedSparseAtlasGridSize != SparseAtlasGridSize ||
		State.AllocatedSparseAtlasBrickCapacity != SparseAtlasBrickCapacity)
	{
		State.SparseFieldAtlasTexture.SafeRelease();
		AllocatePooledTexture(SparseAtlasDesc, State.SparseFieldAtlasTexture, TEXT("TimeThiefSmoke.SparseFieldAtlas"));
		State.AllocatedSparseAtlasBrickGridSize = SparseAtlasBrickGridSize;
		State.AllocatedSparseAtlasGridSize = SparseAtlasGridSize;
		State.AllocatedSparseAtlasBrickCapacity = SparseAtlasBrickCapacity;
		State.bSparseAtlasScatterPending = bSparseBackend;
	}

	EnsureObstacleFieldTextures(GraphBuilder, State);
}

void FTimeThiefSmokeViewExtension::EnsureObstacleFieldTextures(FRDGBuilder& GraphBuilder, FRenderSmokeState& State)
{
	const int32 ObstacleResolution = FMath::Max(
		State.Volume.ObstacleFieldResolution > 0 ? State.Volume.ObstacleFieldResolution : TimeThiefSmokeParameterDefaults::ObstacleMaskResolution,
		1);
	const FIntVector DesiredGridSize(
		ObstacleResolution,
		ObstacleResolution,
		ObstacleResolution);

	if (!State.ObstacleSdfTexture.IsValid() ||
		!State.ObstacleVelocityTexture.IsValid() ||
		!State.ObstacleFaceOpenTexture.IsValid() ||
		State.AllocatedObstacleGridSize != DesiredGridSize)
	{
		State.ObstacleSdfTexture.SafeRelease();
		State.ObstacleVelocityTexture.SafeRelease();
		State.ObstacleFaceOpenTexture.SafeRelease();
		const FRDGTextureDesc ObstacleSdfDesc = FRDGTextureDesc::Create3D(
			DesiredGridSize,
			PF_R16F,
			FClearValueBinding::Black,
			TexCreate_ShaderResource | TexCreate_UAV);
		const FRDGTextureDesc ObstacleVectorDesc = FRDGTextureDesc::Create3D(
			DesiredGridSize,
			PF_FloatRGBA,
			FClearValueBinding::Black,
			TexCreate_ShaderResource | TexCreate_UAV);
		AllocatePooledTexture(ObstacleSdfDesc, State.ObstacleSdfTexture, TEXT("TimeThiefSmoke.ObstacleSdf"));
		AllocatePooledTexture(ObstacleVectorDesc, State.ObstacleVelocityTexture, TEXT("TimeThiefSmoke.ObstacleVelocity"));
		AllocatePooledTexture(ObstacleVectorDesc, State.ObstacleFaceOpenTexture, TEXT("TimeThiefSmoke.ObstacleFaceOpen"));
		State.AllocatedObstacleGridSize = DesiredGridSize;
		State.UploadedObstacleFieldRevision = MAX_uint32;
	}

	if (!State.ObstacleSdfTexture.IsValid() || !State.ObstacleVelocityTexture.IsValid() || !State.ObstacleFaceOpenTexture.IsValid())
	{
		return;
	}

	if (State.UploadedObstacleFieldRevision == State.Volume.ObstacleFieldRevision)
	{
		return;
	}

	const int32 PrimitiveCount = FMath::Min(State.Volume.ObstaclePrimitives.Num(), TimeThiefSmokeParameterDefaults::MaxObstaclePrimitives);
	const int32 UploadPrimitiveCount = FMath::Max(PrimitiveCount, 1);
	FRDGUploadData<FTimeThiefSmokeObstaclePrimitive> UploadData(GraphBuilder, UploadPrimitiveCount);
	for (int32 PrimitiveIndex = 0; PrimitiveIndex < UploadPrimitiveCount; ++PrimitiveIndex)
	{
		UploadData[PrimitiveIndex] = PrimitiveIndex < PrimitiveCount
			? State.Volume.ObstaclePrimitives[PrimitiveIndex]
			: FTimeThiefSmokeObstaclePrimitive();
	}

	FRDGBufferRef PrimitiveBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredUploadDesc(sizeof(FTimeThiefSmokeObstaclePrimitive), UploadPrimitiveCount),
		TEXT("TimeThiefSmoke.ObstaclePrimitiveUpload"));
	GraphBuilder.QueueBufferUpload(PrimitiveBuffer, UploadData, ERDGInitialDataFlags::NoCopy);

	FRDGTextureRef ObstacleSdfTexture = GraphBuilder.RegisterExternalTexture(State.ObstacleSdfTexture, TEXT("TimeThiefSmoke.ObstacleSdf"));
	FRDGTextureRef ObstacleVelocityTexture = GraphBuilder.RegisterExternalTexture(State.ObstacleVelocityTexture, TEXT("TimeThiefSmoke.ObstacleVelocity"));
	FRDGTextureRef ObstacleFaceOpenTexture = GraphBuilder.RegisterExternalTexture(State.ObstacleFaceOpenTexture, TEXT("TimeThiefSmoke.ObstacleFaceOpen"));

	FTimeThiefSmokeBuildObstacleFieldCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeBuildObstacleFieldCS::FParameters>();
	PassParameters->GridResolution = DesiredGridSize;
	PassParameters->BoundsExtent = FVector3f(State.Volume.BoundsExtent);
	PassParameters->ObstaclePrimitives = GraphBuilder.CreateSRV(PrimitiveBuffer);
	PassParameters->ObstaclePrimitiveCount = PrimitiveCount;
	PassParameters->FarDistanceCm = TimeThiefSmokeParameterDefaults::ObstacleFieldFarDistanceCm;
	PassParameters->SurfaceFeatherCm = TimeThiefSmokeParameterDefaults::ObstacleSdfSurfaceFeatherCm;
	PassParameters->OutObstacleSdfTexture = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ObstacleSdfTexture));
	PassParameters->OutObstacleVelocityTexture = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ObstacleVelocityTexture));
	PassParameters->OutObstacleFaceOpenTexture = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ObstacleFaceOpenTexture));

	TShaderMapRef<FTimeThiefSmokeBuildObstacleFieldCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.BuildObstacleField"),
		ComputeShader,
		PassParameters,
		MakeGroupCount(PassParameters->GridResolution));

	State.UploadedObstacleFieldRevision = State.Volume.ObstacleFieldRevision;
}

void FTimeThiefSmokeViewExtension::EnsureVortexParticleBuffers(FRDGBuilder& GraphBuilder, FRenderSmokeState& State)
{
	const int32 DesiredCount = FMath::Clamp(TimeThiefSmokeParameterDefaults::VortexParticleCount, 1, TimeThiefSmokeParameterDefaults::MaxVortexParticleCount);
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
	const int32 ParticleCount = FMath::Clamp(State.AllocatedVortexParticleCount, 1, TimeThiefSmokeParameterDefaults::MaxVortexParticleCount);
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
	FRDGTextureRef DisplacedDensityIn,
	FRDGTextureRef VelocityIn,
	FRDGTextureRef BulletCutoutTexture,
	FRDGTextureRef BulletSinkTexture,
	FRDGBufferRef EventBuffer,
	int32 EventCount,
	float DeltaSeconds)
{
	const int32 VortexParticleCount = FMath::Clamp(TimeThiefSmokeParameterDefaults::VortexParticleCount, 1, TimeThiefSmokeParameterDefaults::MaxVortexParticleCount);
	TShaderMapRef<FTimeThiefSmokeUpdateVortexParticlesCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokeUpdateVortexParticlesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeUpdateVortexParticlesCS::FParameters>();
	PassParameters->VortexParticlesIn = GraphBuilder.CreateSRV(VortexIn);
	PassParameters->OutVortexParticles = GraphBuilder.CreateUAV(VortexOut);
	PassParameters->DensityIn = DensityIn;
	PassParameters->DisplacedDensityIn = DisplacedDensityIn;
	PassParameters->VelocityIn = VelocityIn;
	TIME_THIEF_SMOKE_SET_OBSTACLE_FIELD_PARAMETERS(PassParameters, GraphBuilder.RegisterExternalTexture(State.ObstacleSdfTexture), GraphBuilder, State);
	PassParameters->BulletCutoutTexture = BulletCutoutTexture;
	PassParameters->BulletSinkTexture = BulletSinkTexture;
	PassParameters->VolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->Events = GraphBuilder.CreateSRV(EventBuffer);
	PassParameters->GridResolution = State.AllocatedGridSize;
	PassParameters->BoundsExtent = FVector3f(State.Volume.BoundsExtent);
	PassParameters->DeltaSeconds = DeltaSeconds;
	PassParameters->AgeSeconds = State.Volume.AgeSeconds;
	PassParameters->VortexParticleCount = VortexParticleCount;
	PassParameters->VortexParticleLifeSeconds = FMath::Max(TimeThiefSmokeParameterDefaults::VortexParticleMinLifeSeconds, TimeThiefSmokeParameterDefaults::VortexParticleLifeSeconds);
	PassParameters->VortexParticleStrength = TimeThiefSmokeParameterDefaults::VortexParticleStrength;
	PassParameters->VortexDensityGradientScale = FMath::Max(0.0f, TimeThiefSmokeParameterDefaults::VortexDensityGradientScale);
	PassParameters->SelfWobbleTimeScale = TimeThiefSmokeParameterDefaults::SelfWobbleTimeScale;
	PassParameters->SelfWobbleParticleScale = TimeThiefSmokeParameterDefaults::SelfWobbleParticleScale;
	PassParameters->ActorAirflowMinSpeed = FMath::Max(0.0f, TimeThiefSmokeParameterDefaults::ActorAirflowMinSpeed);
	PassParameters->ActorAirflowFullSpeed = FMath::Max(PassParameters->ActorAirflowMinSpeed + TimeThiefSmokeParameterDefaults::ActorAirflowFullSpeedMinGap, TimeThiefSmokeParameterDefaults::ActorAirflowFullSpeed);
	PassParameters->ActorWakeTrailLengthScale = TimeThiefSmokeParameterDefaults::ActorWakeTrailLengthScale;
	PassParameters->ActorWakeStreetLaneInnerRadiusScale = TimeThiefSmokeParameterDefaults::ActorWakeStreetLaneInnerRadiusScale;
	PassParameters->EventCount = EventCount;
	PassParameters->MaxShaderEventCount = TimeThiefSmokeParameterDefaults::ShaderEventLoopMaxCount;
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
	PassParameters->VortexParticleCount = FMath::Clamp(TimeThiefSmokeParameterDefaults::VortexParticleCount, 1, TimeThiefSmokeParameterDefaults::MaxVortexParticleCount);
	PassParameters->MaxVortexParticleCount = TimeThiefSmokeParameterDefaults::MaxVortexParticleCount;
	PassParameters->VortexParticleSplatRadius = FMath::Max(TimeThiefSmokeParameterDefaults::VortexParticleMinRadius, TimeThiefSmokeParameterDefaults::VortexParticleSplatRadius);
	PassParameters->BrickGridResolution = BrickGridSize;
	PassParameters->SmokeBrickSize = FMath::Clamp(TimeThiefSmokeParameterDefaults::SmokeBrickSize, TimeThiefSmokeParameterDefaults::SmokeBrickMinSize, TimeThiefSmokeParameterDefaults::SmokeBrickMaxSize);
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
	FRDGTextureRef DisplacedDensityIn,
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
	PassParameters->DisplacedDensityIn = DisplacedDensityIn;
	PassParameters->VelocityIn = VelocityIn;
	TIME_THIEF_SMOKE_SET_OBSTACLE_FIELD_PARAMETERS(PassParameters, GraphBuilder.RegisterExternalTexture(State.ObstacleSdfTexture), GraphBuilder, State);
	PassParameters->BrickOccupancyTexture = GraphBuilder.RegisterExternalTexture(State.BrickOccupancyTexture);
	PassParameters->BulletCutoutTexture = BulletCutoutTexture;
	PassParameters->BulletSinkTexture = BulletSinkTexture;
	PassParameters->VolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->OutVelocity = GraphBuilder.CreateUAV(VelocityOut);
	PassParameters->GridResolution = GridSize;
	PassParameters->BoundsExtent = FVector3f(State.Volume.BoundsExtent);
	PassParameters->VortexParticleCount = FMath::Clamp(TimeThiefSmokeParameterDefaults::VortexParticleCount, 1, TimeThiefSmokeParameterDefaults::MaxVortexParticleCount);
	PassParameters->MaxVortexParticleCount = TimeThiefSmokeParameterDefaults::MaxVortexParticleCount;
	PassParameters->VortexParticleSplatRadius = FMath::Max(TimeThiefSmokeParameterDefaults::VortexParticleMinRadius, TimeThiefSmokeParameterDefaults::VortexParticleSplatRadius);
	PassParameters->VortexParticleCoreRadius = FMath::Max(TimeThiefSmokeParameterDefaults::VortexParticleMinRadius, TimeThiefSmokeParameterDefaults::VortexParticleCoreRadius);
	PassParameters->MaxSmokeVelocity = TimeThiefSmokeParameterDefaults::MaxSmokeVelocity;
	PassParameters->BrickGridResolution = State.AllocatedBrickGridSize;
	PassParameters->SmokeBrickSize = FMath::Clamp(TimeThiefSmokeParameterDefaults::SmokeBrickSize, TimeThiefSmokeParameterDefaults::SmokeBrickMinSize, TimeThiefSmokeParameterDefaults::SmokeBrickMaxSize);
	PassParameters->bUseVortexBrickBins = TimeThiefSmokeParameterDefaults::bUseVortexBrickBins ? 1u : 0u;
	PassParameters->bUseSparseSimulationMask = State.bUseSparseSimulationMaskThisFrame ? 1u : 0u;

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.SplatVortexParticles SmokeId=%d", State.Volume.SmokeId),
		ComputeShader,
		PassParameters,
		GroupCount);
}

void FTimeThiefSmokeViewExtension::AddBuildEventBrickMasksPass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGBufferRef AdvectionEventBuffer,
	int32 AdvectionEventCount,
	FRDGBufferRef ExplosionEventBuffer,
	int32 ExplosionEventCount,
	FRDGBufferRef ActorEventBuffer,
	int32 ActorEventCount,
	FRDGBufferRef VortexEventBuffer,
	int32 VortexEventCount,
	FRDGBufferRef& EmptyEventBuffer,
	FRDGBufferRef& EmptyEventBrickMasksBuffer,
	FRDGBufferRef& OutAdvectionEventBrickMasksBuffer,
	FRDGBufferRef& OutExplosionEventBrickMasksBuffer,
	FRDGBufferRef& OutActorEventBrickMasksBuffer,
	FRDGBufferRef& OutVortexEventBrickMasksBuffer)
{
	const int32 EventCounts[] =
	{
		FMath::Min(AdvectionEventCount, TimeThiefSmokeParameterDefaults::MaxShaderEventCount),
		FMath::Min(ExplosionEventCount, TimeThiefSmokeParameterDefaults::MaxShaderEventCount),
		FMath::Min(ActorEventCount, TimeThiefSmokeParameterDefaults::MaxShaderEventCount),
		FMath::Min(VortexEventCount, TimeThiefSmokeParameterDefaults::MaxShaderEventCount)
	};
	const bool bHasAnyEventMask = EventCounts[0] > 0 || EventCounts[1] > 0 || EventCounts[2] > 0 || EventCounts[3] > 0;

	const int32 BrickMaskCount = FMath::Max(1, State.AllocatedBrickGridSize.X * State.AllocatedBrickGridSize.Y * State.AllocatedBrickGridSize.Z);
	FRDGBufferRef EmptyWritableMaskBuffer = nullptr;
	auto GetEmptyWritableMaskBuffer = [&]() -> FRDGBufferRef
	{
		if (!EmptyWritableMaskBuffer)
		{
			TArray<uint32> EmptyMask;
			EmptyMask.AddZeroed(4);
			FRDGBufferDesc EmptyDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32) * 4, 1);
			EmptyDesc.Usage |= EBufferUsageFlags::ShaderResource | EBufferUsageFlags::UnorderedAccess;
			EmptyWritableMaskBuffer = GraphBuilder.CreateBuffer(EmptyDesc, TEXT("TimeThiefSmoke.EmptyWritableEventBrickMasks"));
			GraphBuilder.QueueBufferUpload(EmptyWritableMaskBuffer, EmptyMask.GetData(), EmptyMask.Num() * sizeof(uint32));
		}
		return EmptyWritableMaskBuffer;
	};
	auto CreateMaskBuffer = [&](const int32 EventCount, const TCHAR* Name) -> FRDGBufferRef
	{
		if (EventCount <= 0)
		{
			if (!bHasAnyEventMask)
			{
				return GetEmptyBrickMaskBuffer(GraphBuilder, EmptyEventBrickMasksBuffer);
			}

			return GetEmptyWritableMaskBuffer();
		}

		FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32) * 4, BrickMaskCount);
		Desc.Usage |= EBufferUsageFlags::ShaderResource | EBufferUsageFlags::UnorderedAccess;
		return GraphBuilder.CreateBuffer(Desc, Name);
	};

	OutAdvectionEventBrickMasksBuffer = CreateMaskBuffer(EventCounts[0], TEXT("TimeThiefSmoke.AdvectionEventBrickMasks"));
	OutExplosionEventBrickMasksBuffer = CreateMaskBuffer(EventCounts[1], TEXT("TimeThiefSmoke.ExplosionEventBrickMasks"));
	OutActorEventBrickMasksBuffer = CreateMaskBuffer(EventCounts[2], TEXT("TimeThiefSmoke.ActorEventBrickMasks"));
	OutVortexEventBrickMasksBuffer = CreateMaskBuffer(EventCounts[3], TEXT("TimeThiefSmoke.VortexEventBrickMasks"));
	if (!bHasAnyEventMask)
	{
		return;
	}

	TShaderMapRef<FTimeThiefSmokeBuildEventBrickMasksCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokeBuildEventBrickMasksCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeBuildEventBrickMasksCS::FParameters>();
	PassParameters->GridResolution = State.AllocatedGridSize;
	PassParameters->BrickGridResolution = State.AllocatedBrickGridSize;
	PassParameters->SmokeBrickSize = FMath::Clamp(TimeThiefSmokeParameterDefaults::SmokeBrickSize, TimeThiefSmokeParameterDefaults::SmokeBrickMinSize, TimeThiefSmokeParameterDefaults::SmokeBrickMaxSize);
	PassParameters->BoundsExtent = FVector3f(State.Volume.BoundsExtent);
	PassParameters->EventCount0 = EventCounts[0];
	PassParameters->EventCount1 = EventCounts[1];
	PassParameters->EventCount2 = EventCounts[2];
	PassParameters->EventCount3 = EventCounts[3];
	PassParameters->MaxShaderEventCount = TimeThiefSmokeParameterDefaults::ShaderEventLoopMaxCount;
	PassParameters->LocalToWorld = State.Volume.LocalToWorld.ToMatrixWithScale();
	FRDGBufferRef EmptyEvents = GetEmptySmokeEventBuffer(GraphBuilder, EmptyEventBuffer);
	PassParameters->Events0 = GraphBuilder.CreateSRV(EventCounts[0] > 0 ? AdvectionEventBuffer : EmptyEvents);
	PassParameters->Events1 = GraphBuilder.CreateSRV(EventCounts[1] > 0 ? ExplosionEventBuffer : EmptyEvents);
	PassParameters->Events2 = GraphBuilder.CreateSRV(EventCounts[2] > 0 ? ActorEventBuffer : EmptyEvents);
	PassParameters->Events3 = GraphBuilder.CreateSRV(EventCounts[3] > 0 ? VortexEventBuffer : EmptyEvents);
	PassParameters->ActorAirflowRadiusScale = FMath::Max(TimeThiefSmokeParameterDefaults::ActorAirflowRadiusScaleMin, TimeThiefSmokeParameterDefaults::ActorAirflowRadiusScale);
	PassParameters->ActorWakeTrailLengthScale = TimeThiefSmokeParameterDefaults::ActorWakeTrailLengthScale;
	PassParameters->BulletWakeCutoutFeather = FMath::Max(TimeThiefSmokeParameterDefaults::BulletWakeCutoutFeatherMin, TimeThiefSmokeParameterDefaults::BulletWakeCutoutFeather);
	PassParameters->OutEventBrickMasks0 = GraphBuilder.CreateUAV(OutAdvectionEventBrickMasksBuffer);
	PassParameters->OutEventBrickMasks1 = GraphBuilder.CreateUAV(OutExplosionEventBrickMasksBuffer);
	PassParameters->OutEventBrickMasks2 = GraphBuilder.CreateUAV(OutActorEventBrickMasksBuffer);
	PassParameters->OutEventBrickMasks3 = GraphBuilder.CreateUAV(OutVortexEventBrickMasksBuffer);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.BuildEventBrickMasks SmokeId=%d Events=%d/%d/%d/%d", State.Volume.SmokeId, EventCounts[0], EventCounts[1], EventCounts[2], EventCounts[3]),
		ComputeShader,
		PassParameters,
		MakeGroupCount(State.AllocatedBrickGridSize));
}

void FTimeThiefSmokeViewExtension::SimulateSmoke(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	float DeltaSeconds)
{
	EnsureResources(GraphBuilder, State);
	EnsureVortexParticleBuffers(GraphBuilder, State);
	const bool bUseMacProjection = GetDefaultSimulationBackend() == ETimeThiefSmokeSimulationBackend::SparseMac;

	FRDGTextureRef DensityTextures[2] =
	{
		GraphBuilder.RegisterExternalTexture(State.DensityTextures[0]),
		GraphBuilder.RegisterExternalTexture(State.DensityTextures[1])
	};
	FRDGTextureRef DisplacedDensityTextures[2] =
	{
		GraphBuilder.RegisterExternalTexture(State.DisplacedDensityTextures[0]),
		GraphBuilder.RegisterExternalTexture(State.DisplacedDensityTextures[1])
	};
	FRDGTextureRef VelocityTextures[2] =
	{
		GraphBuilder.RegisterExternalTexture(State.VelocityTextures[0]),
		GraphBuilder.RegisterExternalTexture(State.VelocityTextures[1])
	};
	const FRDGTextureDesc ScratchScalarDesc = FRDGTextureDesc::Create3D(
		State.AllocatedGridSize,
		PF_R16F,
		FClearValueBinding::Black,
		TexCreate_ShaderResource | TexCreate_UAV);
	const FRDGTextureDesc ScratchVectorDesc = FRDGTextureDesc::Create3D(
		State.AllocatedGridSize,
		PF_FloatRGBA,
		FClearValueBinding::Black,
		TexCreate_ShaderResource | TexCreate_UAV);
	FRDGTextureRef MacVelocityUTextures[2] = {};
	FRDGTextureRef MacVelocityVTextures[2] = {};
	FRDGTextureRef MacVelocityWTextures[2] = {};
	if (bUseMacProjection)
	{
		MacVelocityUTextures[0] = GraphBuilder.CreateTexture(ScratchScalarDesc, TEXT("TimeThiefSmoke.MacVelocityU"));
		MacVelocityVTextures[0] = GraphBuilder.CreateTexture(ScratchScalarDesc, TEXT("TimeThiefSmoke.MacVelocityV"));
		MacVelocityWTextures[0] = GraphBuilder.CreateTexture(ScratchScalarDesc, TEXT("TimeThiefSmoke.MacVelocityW"));
	}
	FRDGTextureRef PressureTextures[2] =
	{
		GraphBuilder.CreateTexture(ScratchScalarDesc, TEXT("TimeThiefSmoke.Pressure0")),
		GraphBuilder.CreateTexture(ScratchScalarDesc, TEXT("TimeThiefSmoke.Pressure1"))
	};
	FRDGTextureRef CurlTexture = GraphBuilder.CreateTexture(ScratchVectorDesc, TEXT("TimeThiefSmoke.Curl"));
	FRDGTextureRef DivergenceTexture = GraphBuilder.CreateTexture(ScratchScalarDesc, TEXT("TimeThiefSmoke.Divergence"));
	FRDGTextureRef ObstacleTexture = GraphBuilder.RegisterExternalTexture(State.ObstacleSdfTexture);
	FRDGBufferRef VortexParticleBuffers[2] =
	{
		GraphBuilder.RegisterExternalBuffer(State.VortexParticleBuffers[0], TEXT("TimeThiefSmoke.VortexParticles0")),
		GraphBuilder.RegisterExternalBuffer(State.VortexParticleBuffers[1], TEXT("TimeThiefSmoke.VortexParticles1"))
	};
	FRDGBufferRef EmptyEventBuffer = nullptr;
	FRDGBufferRef EmptyEventBrickMasksBuffer = nullptr;
	TArray<FTimeThiefSmokeRendererEvent> AdvectionEvents;
	TArray<FTimeThiefSmokeRendererEvent> ExplosionEvents;
	TArray<FTimeThiefSmokeRendererEvent> ActorEvents;
	TArray<FTimeThiefSmokeRendererEvent> VortexEvents;
	AdvectionEvents.Reserve(State.PendingEvents.Num());
	ExplosionEvents.Reserve(FMath::Min(State.PendingEvents.Num(), TimeThiefSmokeParameterDefaults::MaxSimulationExplosionEventCount));
	ActorEvents.Reserve(FMath::Min(State.PendingEvents.Num(), TimeThiefSmokeParameterDefaults::MaxSimulationActorEventCount));
	VortexEvents.Reserve(FMath::Min(State.PendingEvents.Num(), TimeThiefSmokeParameterDefaults::MaxSimulationVortexEventCount));
	for (const FTimeThiefSmokeRendererEvent& Event : State.PendingEvents)
	{
		if (!IsSimulationEventActive(Event))
		{
			continue;
		}

		AdvectionEvents.Add(Event);
		if (Event.Type == ETimeThiefSmokeRendererInteractionType::ExplosionShock)
		{
			ExplosionEvents.Add(Event);
		}
		else if (Event.Type == ETimeThiefSmokeRendererInteractionType::ActorPush)
		{
			ActorEvents.Add(Event);
		}
		if (IsVortexPipelineEvent(Event))
		{
			VortexEvents.Add(Event);
		}
	}

	SortAndClampSmokeEvents(AdvectionEvents, TimeThiefSmokeParameterDefaults::MaxShaderEventCount);
	ClampSmokeEventsByType(AdvectionEvents, ETimeThiefSmokeRendererInteractionType::BulletWake, TimeThiefSmokeParameterDefaults::MaxSimulationBulletEventCount);
	SortAndClampSmokeEvents(ExplosionEvents, TimeThiefSmokeParameterDefaults::MaxSimulationExplosionEventCount);
	SortAndClampSmokeEvents(ActorEvents, TimeThiefSmokeParameterDefaults::MaxSimulationActorEventCount);
	SortAndClampSmokeEvents(VortexEvents, TimeThiefSmokeParameterDefaults::MaxSimulationVortexEventCount);

	int32 BulletEventCount = 0;
	for (const FTimeThiefSmokeRendererEvent& Event : AdvectionEvents)
	{
		if (Event.Type == ETimeThiefSmokeRendererInteractionType::BulletWake)
		{
			++BulletEventCount;
		}
	}
	BulletEventCount = FMath::Min(BulletEventCount, TimeThiefSmokeParameterDefaults::MaxSimulationBulletEventCount);
	const int32 AdvectionEventCount = FMath::Min(AdvectionEvents.Num(), TimeThiefSmokeParameterDefaults::MaxShaderEventCount);
	const int32 ExplosionEventCount = FMath::Min(ExplosionEvents.Num(), TimeThiefSmokeParameterDefaults::MaxShaderEventCount);
	const int32 ActorEventCount = FMath::Min(ActorEvents.Num(), TimeThiefSmokeParameterDefaults::MaxShaderEventCount);
	const int32 VortexEventCount = FMath::Min(VortexEvents.Num(), TimeThiefSmokeParameterDefaults::MaxShaderEventCount);
	const bool bHasDynamicSimulationEvent = BulletEventCount > 0 || ExplosionEventCount > 0 || ActorEventCount > 0;
	bool bAllocatedBulletFieldsThisFrame = false;
	if (BulletEventCount > 0)
	{
		for (int32 TextureIndex = 0; TextureIndex < 2; ++TextureIndex)
		{
			if (!State.BulletCutoutTextures[TextureIndex].IsValid())
			{
				AllocatePooledTexture(ScratchScalarDesc, State.BulletCutoutTextures[TextureIndex], TEXT("TimeThiefSmoke.BulletCutout"));
				bAllocatedBulletFieldsThisFrame = true;
			}
			if (!State.BulletSinkTextures[TextureIndex].IsValid())
			{
				AllocatePooledTexture(ScratchScalarDesc, State.BulletSinkTextures[TextureIndex], TEXT("TimeThiefSmoke.BulletSink"));
				bAllocatedBulletFieldsThisFrame = true;
			}
		}
		if (bAllocatedBulletFieldsThisFrame)
		{
			State.CurrentBulletFieldIndex = 0;
		}
	}
	const bool bHasBulletFieldTextures =
		State.BulletCutoutTextures[0].IsValid() &&
		State.BulletCutoutTextures[1].IsValid() &&
		State.BulletSinkTextures[0].IsValid() &&
		State.BulletSinkTextures[1].IsValid();
	FRDGTextureRef BulletCutoutTextures[2] = {};
	FRDGTextureRef BulletSinkTextures[2] = {};
	if (bHasBulletFieldTextures)
	{
		BulletCutoutTextures[0] = GraphBuilder.RegisterExternalTexture(State.BulletCutoutTextures[0]);
		BulletCutoutTextures[1] = GraphBuilder.RegisterExternalTexture(State.BulletCutoutTextures[1]);
		BulletSinkTextures[0] = GraphBuilder.RegisterExternalTexture(State.BulletSinkTextures[0]);
		BulletSinkTextures[1] = GraphBuilder.RegisterExternalTexture(State.BulletSinkTextures[1]);
		if (bAllocatedBulletFieldsThisFrame)
		{
			AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(BulletCutoutTextures[0]), 0.0f);
			AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(BulletCutoutTextures[1]), 0.0f);
			AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(BulletSinkTextures[0]), 0.0f);
			AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(BulletSinkTextures[1]), 0.0f);
		}
	}
	else
	{
		FRDGTextureRef ZeroBulletTexture = GSystemTextures.GetDefaultTexture(GraphBuilder, ETextureDimension::Texture3D, PF_R16F, FClearValueBinding::Black);
		BulletCutoutTextures[0] = ZeroBulletTexture;
		BulletCutoutTextures[1] = ZeroBulletTexture;
		BulletSinkTextures[0] = ZeroBulletTexture;
		BulletSinkTextures[1] = ZeroBulletTexture;
	}
	FRDGBufferRef AdvectionEventBuffer = nullptr;
	FRDGBufferRef ExplosionEventBuffer = nullptr;
	FRDGBufferRef ActorEventBuffer = nullptr;
	FRDGBufferRef VortexEventBuffer = nullptr;
	FRDGBufferRef AdvectionEventBrickMasksBuffer = nullptr;
	FRDGBufferRef ExplosionEventBrickMasksBuffer = nullptr;
	FRDGBufferRef ActorEventBrickMasksBuffer = nullptr;
	FRDGBufferRef VortexEventBrickMasksBuffer = nullptr;
	auto GetAdvectionEventBuffer = [&]() -> FRDGBufferRef
	{
		if (!AdvectionEventBuffer)
		{
			int32 IgnoredEventCount = 0;
			AdvectionEventBuffer = CreateSmokeEventBuffer(GraphBuilder, AdvectionEvents, IgnoredEventCount, EmptyEventBuffer, TEXT("TimeThiefSmoke.AdvectionEvents"));
		}
		return AdvectionEventBuffer;
	};
	auto GetExplosionEventBuffer = [&]() -> FRDGBufferRef
	{
		if (!ExplosionEventBuffer)
		{
			int32 IgnoredEventCount = 0;
			ExplosionEventBuffer = CreateSmokeEventBuffer(GraphBuilder, ExplosionEvents, IgnoredEventCount, EmptyEventBuffer, TEXT("TimeThiefSmoke.ExplosionEvents"));
		}
		return ExplosionEventBuffer;
	};
	auto GetActorEventBuffer = [&]() -> FRDGBufferRef
	{
		if (!ActorEventBuffer)
		{
			int32 IgnoredEventCount = 0;
			ActorEventBuffer = CreateSmokeEventBuffer(GraphBuilder, ActorEvents, IgnoredEventCount, EmptyEventBuffer, TEXT("TimeThiefSmoke.ActorEvents"));
		}
		return ActorEventBuffer;
	};
	auto GetVortexEventBuffer = [&]() -> FRDGBufferRef
	{
		if (!VortexEventBuffer)
		{
			int32 IgnoredEventCount = 0;
			VortexEventBuffer = CreateSmokeEventBuffer(GraphBuilder, VortexEvents, IgnoredEventCount, EmptyEventBuffer, TEXT("TimeThiefSmoke.VortexEvents"));
		}
		return VortexEventBuffer;
	};
	bool bEventBrickMasksBuilt = false;
	auto BuildEventBrickMasks = [&]()
	{
		if (bEventBrickMasksBuilt)
		{
			return;
		}

		AddBuildEventBrickMasksPass(
			GraphBuilder,
			State,
			GetAdvectionEventBuffer(),
			AdvectionEventCount,
			GetExplosionEventBuffer(),
			ExplosionEventCount,
			GetActorEventBuffer(),
			ActorEventCount,
			GetVortexEventBuffer(),
			VortexEventCount,
			EmptyEventBuffer,
			EmptyEventBrickMasksBuffer,
			AdvectionEventBrickMasksBuffer,
			ExplosionEventBrickMasksBuffer,
			ActorEventBrickMasksBuffer,
			VortexEventBrickMasksBuffer);
		bEventBrickMasksBuilt = true;
	};
	auto GetAdvectionEventBrickMasksBuffer = [&]() -> FRDGBufferRef
	{
		BuildEventBrickMasks();
		return AdvectionEventBrickMasksBuffer;
	};
	auto GetExplosionEventBrickMasksBuffer = [&]() -> FRDGBufferRef
	{
		BuildEventBrickMasks();
		return ExplosionEventBrickMasksBuffer;
	};
	auto GetActorEventBrickMasksBuffer = [&]() -> FRDGBufferRef
	{
		BuildEventBrickMasks();
		return ActorEventBrickMasksBuffer;
	};
	auto GetVortexEventBrickMasksBuffer = [&]() -> FRDGBufferRef
	{
		BuildEventBrickMasks();
		return VortexEventBrickMasksBuffer;
	};
	const int32 SparseMaskEventCount = AdvectionEventCount;
	const bool bNeedsInitThisFrame = State.bNeedsInit;
	const bool bHasExplosionEvent = ExplosionEventCount > 0;
	const bool bHasActorEvent = ActorEventCount > 0;
	const bool bHasSimulationEvent = AdvectionEventCount > 0;
	const bool bHasBulletStateForOccupancy = BulletEventCount > 0 || State.BulletFieldDecayBudgetSeconds > UE_SMALL_NUMBER;
	State.bUseSparseSimulationMaskThisFrame = GetDefaultSimulationBackend() == ETimeThiefSmokeSimulationBackend::SparseMac &&
		!bNeedsInitThisFrame &&
		(HasRenderableSparseState(State.SparseActiveBrickCount, State.bSparseActiveBrickCountReadbackPending) || bHasSimulationEvent) &&
		State.AllocatedBrickGridSize != FIntVector(1, 1, 1);
	const bool bRefreshSparseSimulationMask = ShouldRefreshSparseSimulationMask(
		State.bUseSparseSimulationMaskThisFrame,
		bHasDynamicSimulationEvent,
		State.SparseActiveBrickCount,
		State.bSparseActiveBrickCountReadbackPending,
		State.bSparseOccupancyRefreshPending);
	FRDGTextureRef SparseSimulationBrickOccupancyTexture = State.bUseSparseSimulationMaskThisFrame
		? GraphBuilder.RegisterExternalTexture(State.BrickOccupancyTexture)
		: nullptr;
	FActiveBrickDispatchResources SparseSimulationActiveBricks;
	bool bHasSparseSimulationActiveBricks = false;
	if (bRefreshSparseSimulationMask)
	{
		FRDGTextureRef BrickActivityTexture = CreateTransientUIntTexture(
			GraphBuilder,
			State.AllocatedBrickGridSize,
			TEXT("TimeThiefSmoke.PreEventBrickActivity"));
		AddBuildBrickOccupancyPass(
			GraphBuilder,
			State,
			DensityTextures[State.CurrentDensityIndex],
			DisplacedDensityTextures[State.CurrentDensityIndex],
			VelocityTextures[State.CurrentVelocityIndex],
			BulletCutoutTextures[State.CurrentBulletFieldIndex],
			BulletSinkTextures[State.CurrentBulletFieldIndex],
			GetAdvectionEventBuffer(),
			SparseMaskEventCount,
			BrickActivityTexture,
			bHasBulletStateForOccupancy);
		const uint32 SimulationBrickCount = GetBrickGridCount(State.AllocatedBrickGridSize);
		SparseSimulationActiveBricks = AddExpandBrickOccupancyPass(
			GraphBuilder,
			State,
			BrickActivityTexture,
			SparseSimulationBrickOccupancyTexture,
			SimulationBrickCount,
			SimulationBrickCount);
		bHasSparseSimulationActiveBricks = true;
		QueueSparseActiveBrickCountReadback(GraphBuilder, State, SparseSimulationActiveBricks.ActiveBrickCountBuffer);
		State.bSparseOccupancyRefreshPending = false;
	}
	const FActiveBrickDispatchResources* SparseSimulationActiveBricksPtr = nullptr;
	if (State.bUseSparseSimulationMaskThisFrame)
	{
		if (!bHasSparseSimulationActiveBricks)
		{
			SparseSimulationActiveBricks = AddBuildActiveBrickListPass(
				GraphBuilder,
				State,
				SparseSimulationBrickOccupancyTexture);
			bHasSparseSimulationActiveBricks = true;
			QueueSparseActiveBrickCountReadback(GraphBuilder, State, SparseSimulationActiveBricks.ActiveBrickCountBuffer);
		}
		SparseSimulationActiveBricksPtr = &SparseSimulationActiveBricks;
	}
	FActiveBrickDispatchResources EmptyActiveBrickResources;
	auto GetActiveBrickResourcesForPass = [&]() -> const FActiveBrickDispatchResources*
	{
		if (SparseSimulationActiveBricksPtr)
		{
			return SparseSimulationActiveBricksPtr;
		}

		if (State.bUseSparseSimulationMaskThisFrame)
		{
			return nullptr;
		}

		if (!EmptyActiveBrickResources.ActiveBrickCountBuffer || !EmptyActiveBrickResources.ActiveBricksBuffer)
		{
			CreateEmptyActiveBrickBuffers(
				GraphBuilder,
				EmptyActiveBrickResources.ActiveBrickCountBuffer,
				EmptyActiveBrickResources.ActiveBricksBuffer);
		}

		return &EmptyActiveBrickResources;
	};
	const bool bHadBulletFieldForAtlas = State.BulletFieldDecayBudgetSeconds > UE_SMALL_NUMBER;
	if (BulletEventCount > 0)
	{
		const float BulletKeepAliveSeconds = FMath::Max3(
			FMath::Max(TimeThiefSmokeParameterDefaults::BulletWakeMinLifeSeconds, TimeThiefSmokeParameterDefaults::BulletWakeMaxVisibleLife),
			FMath::Max(TimeThiefSmokeParameterDefaults::BulletWakeMinLifeSeconds, TimeThiefSmokeParameterDefaults::BulletWakeReleaseDuration),
			FMath::Max(TimeThiefSmokeParameterDefaults::BulletWakeMinLifeSeconds, TimeThiefSmokeParameterDefaults::BulletWakeSinkLife));
		State.BulletFieldDecayBudgetSeconds = FMath::Max(State.BulletFieldDecayBudgetSeconds, BulletKeepAliveSeconds);
	}
	const bool bVortexUploadPending = State.bVortexParticlesNeedUpload;
	State.AccumulatedVortexDeltaSeconds = FMath::Min(State.AccumulatedVortexDeltaSeconds + DeltaSeconds, TimeThiefSmokeParameterDefaults::VortexSubstepIntervalSeconds * 2.0f);
	const bool bRunVortexPasses = ShouldRunVortexPasses(
		bNeedsInitThisFrame,
		bVortexUploadPending,
		VortexEventCount,
		State.AccumulatedVortexDeltaSeconds);

	if (State.bVortexParticlesNeedUpload)
	{
		UploadDeadVortexParticles(GraphBuilder, State, VortexParticleBuffers[0]);
		UploadDeadVortexParticles(GraphBuilder, State, VortexParticleBuffers[1]);
		State.CurrentVortexParticleIndex = 0;
		State.bVortexParticlesNeedUpload = false;
	}

	if (State.bNeedsInit)
	{
		AddInitPass(GraphBuilder, State, DensityTextures[State.CurrentDensityIndex], DisplacedDensityTextures[State.CurrentDensityIndex], VelocityTextures[State.CurrentVelocityIndex]);
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(DisplacedDensityTextures[0]), 0.0f);
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(DisplacedDensityTextures[1]), 0.0f);
		if (bHasBulletFieldTextures)
		{
			AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(BulletCutoutTextures[0]), 0.0f);
			AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(BulletCutoutTextures[1]), 0.0f);
			AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(BulletSinkTextures[0]), 0.0f);
			AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(BulletSinkTextures[1]), 0.0f);
		}
		State.bNeedsInit = false;
	}

	if (bHasExplosionEvent)
	{
		const int32 EventReadDensityIndex = State.CurrentDensityIndex;
		const int32 EventReadVelocityIndex = State.CurrentVelocityIndex;
		const int32 EventWriteDensityIndex = 1 - State.CurrentDensityIndex;
		const int32 EventWriteVelocityIndex = 1 - State.CurrentVelocityIndex;
		AddApplyEventsPass(
			GraphBuilder,
			State,
			DensityTextures[EventReadDensityIndex],
			DisplacedDensityTextures[EventReadDensityIndex],
			VelocityTextures[EventReadVelocityIndex],
			DensityTextures[EventWriteDensityIndex],
			DisplacedDensityTextures[EventWriteDensityIndex],
			VelocityTextures[EventWriteVelocityIndex],
			GetExplosionEventBuffer(),
			GetExplosionEventBrickMasksBuffer(),
			ExplosionEventCount,
			DeltaSeconds,
			GetActiveBrickResourcesForPass());
		State.CurrentDensityIndex = EventWriteDensityIndex;
		State.CurrentVelocityIndex = EventWriteVelocityIndex;
	}

	if (bHasActorEvent)
	{
		const int32 ObstacleReadDensityIndex = State.CurrentDensityIndex;
		const int32 ObstacleReadVelocityIndex = State.CurrentVelocityIndex;
		const int32 ObstacleWriteDensityIndex = 1 - State.CurrentDensityIndex;
		const int32 ObstacleWriteVelocityIndex = 1 - State.CurrentVelocityIndex;
		AddDynamicObstaclePass(
			GraphBuilder,
			State,
			DensityTextures[ObstacleReadDensityIndex],
			DisplacedDensityTextures[ObstacleReadDensityIndex],
			VelocityTextures[ObstacleReadVelocityIndex],
			DensityTextures[ObstacleWriteDensityIndex],
			DisplacedDensityTextures[ObstacleWriteDensityIndex],
			VelocityTextures[ObstacleWriteVelocityIndex],
			GetActorEventBuffer(),
			GetActorEventBrickMasksBuffer(),
			ActorEventCount,
			DeltaSeconds,
			GetActiveBrickResourcesForPass());
		State.CurrentDensityIndex = ObstacleWriteDensityIndex;
		State.CurrentVelocityIndex = ObstacleWriteVelocityIndex;
	}

	const int32 ReadDensityIndex = State.CurrentDensityIndex;
	const int32 ReadVelocityIndex = State.CurrentVelocityIndex;
	const int32 WriteDensityIndex = 1 - State.CurrentDensityIndex;
	const int32 WriteVelocityIndex = 1 - State.CurrentVelocityIndex;

	const int32 BulletReadIndex = State.CurrentBulletFieldIndex;
	const int32 BulletWriteIndex = 1 - State.CurrentBulletFieldIndex;
	FRDGTextureRef BulletCutoutReadTexture = bHasBulletFieldTextures ? BulletCutoutTextures[BulletReadIndex] : nullptr;
	FRDGTextureRef BulletSinkReadTexture = bHasBulletFieldTextures ? BulletSinkTextures[BulletReadIndex] : nullptr;
	FRDGTextureRef BulletCutoutWriteTexture = bHasBulletFieldTextures ? BulletCutoutTextures[BulletWriteIndex] : nullptr;
	FRDGTextureRef BulletSinkWriteTexture = bHasBulletFieldTextures ? BulletSinkTextures[BulletWriteIndex] : nullptr;
	AddSimulatePass(
		GraphBuilder,
		State,
		DensityTextures[ReadDensityIndex],
		DisplacedDensityTextures[ReadDensityIndex],
		VelocityTextures[ReadVelocityIndex],
		BulletCutoutReadTexture,
		BulletSinkReadTexture,
		GetAdvectionEventBuffer(),
		GetAdvectionEventBrickMasksBuffer(),
		AdvectionEventCount,
		DensityTextures[WriteDensityIndex],
		DisplacedDensityTextures[WriteDensityIndex],
		VelocityTextures[WriteVelocityIndex],
		BulletCutoutWriteTexture,
		BulletSinkWriteTexture,
		DeltaSeconds,
		GetActiveBrickResourcesForPass());
	State.CurrentDensityIndex = WriteDensityIndex;
	State.CurrentVelocityIndex = WriteVelocityIndex;
	if (bHasBulletFieldTextures)
	{
		State.CurrentBulletFieldIndex = BulletWriteIndex;
	}
	if (State.BulletFieldDecayBudgetSeconds > UE_SMALL_NUMBER)
	{
		State.BulletFieldDecayBudgetSeconds = FMath::Max(0.0f, State.BulletFieldDecayBudgetSeconds - DeltaSeconds);
	}

	const bool bScatterBulletChannels = BulletEventCount > 0 || bHadBulletFieldForAtlas;

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
			DisplacedDensityTextures[State.CurrentDensityIndex],
			VelocityTextures[State.CurrentVelocityIndex],
			BulletCutoutTextures[State.CurrentBulletFieldIndex],
			BulletSinkTextures[State.CurrentBulletFieldIndex],
			GetVortexEventBuffer(),
			VortexEventCount,
			VortexDeltaSeconds);
		State.CurrentVortexParticleIndex = VortexParticleWriteIndex;

		const int32 VorticityWriteVelocityIndex = 1 - State.CurrentVelocityIndex;
		AddBuildCurlPass(GraphBuilder, State, VelocityTextures[State.CurrentVelocityIndex], CurlTexture);
		AddVorticityPass(GraphBuilder, State, DensityTextures[State.CurrentDensityIndex], DisplacedDensityTextures[State.CurrentDensityIndex], VelocityTextures[State.CurrentVelocityIndex], CurlTexture, VelocityTextures[VorticityWriteVelocityIndex], GetVortexEventBuffer(), GetVortexEventBrickMasksBuffer(), VortexEventCount, VortexDeltaSeconds);
		State.CurrentVelocityIndex = VorticityWriteVelocityIndex;

		const int32 VortexSplatWriteVelocityIndex = 1 - State.CurrentVelocityIndex;
		FRDGBufferRef VortexBrickMasksBuffer = nullptr;
		if (TimeThiefSmokeParameterDefaults::bUseVortexBrickBins)
		{
			VortexBrickMasksBuffer = AddBuildVortexBrickMasksPass(
				GraphBuilder,
				State,
				VortexParticleBuffers[State.CurrentVortexParticleIndex]);
		}
		else
		{
			VortexBrickMasksBuffer = GetEmptyBrickMaskBuffer(GraphBuilder, EmptyEventBrickMasksBuffer);
		}
		AddVortexParticleSplatPass(
			GraphBuilder,
			State,
			VortexParticleBuffers[State.CurrentVortexParticleIndex],
			VortexBrickMasksBuffer,
			DensityTextures[State.CurrentDensityIndex],
			DisplacedDensityTextures[State.CurrentDensityIndex],
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
		AddBuildMacDivergencePass(
			GraphBuilder,
			State,
			VelocityTextures[State.CurrentVelocityIndex],
			MacVelocityUTextures[0],
			MacVelocityVTextures[0],
			MacVelocityWTextures[0],
			DivergenceTexture,
			PressureTextures[0],
			GetActiveBrickResourcesForPass());
	}
	else
	{
		AddDivergencePass(GraphBuilder, State, VelocityTextures[State.CurrentVelocityIndex], DivergenceTexture, PressureTextures[0]);
	}

	FRDGTextureRef PressureForProjection = PressureTextures[0];
	const FVector3f CellSize = MakeCellSize(State.Volume, State.AllocatedGridSize);
	int32 CurrentPressureIndex = 0;
	const int32 PressureIterations = FMath::Clamp(TimeThiefSmokeParameterDefaults::PressureIterations, TimeThiefSmokeParameterDefaults::PressureIterationsMin, TimeThiefSmokeParameterDefaults::PressureIterationsMax);
	for (int32 Iteration = 0; Iteration < PressureIterations; ++Iteration)
	{
		const int32 NextPressureIndex = 1 - CurrentPressureIndex;
		AddPressureJacobiPass(GraphBuilder, State, State.AllocatedGridSize, CellSize, PressureTextures[CurrentPressureIndex], DivergenceTexture, ObstacleTexture, PressureTextures[NextPressureIndex], GetActiveBrickResourcesForPass());
		CurrentPressureIndex = NextPressureIndex;
	}
	PressureForProjection = PressureTextures[CurrentPressureIndex];

	const int32 ProjectedVelocityIndex = 1 - State.CurrentVelocityIndex;
	if (bUseMacProjection)
	{
		AddProjectMacToCollocatedVelocityPass(
			GraphBuilder,
			State,
			MacVelocityUTextures[0],
			MacVelocityVTextures[0],
			MacVelocityWTextures[0],
			PressureForProjection,
			VelocityTextures[ProjectedVelocityIndex],
			GetActiveBrickResourcesForPass());
	}
	else
	{
		AddProjectVelocityPass(GraphBuilder, State, VelocityTextures[State.CurrentVelocityIndex], PressureForProjection, VelocityTextures[ProjectedVelocityIndex]);
	}
	State.CurrentVelocityIndex = ProjectedVelocityIndex;

	if (GetDefaultSimulationBackend() == ETimeThiefSmokeSimulationBackend::SparseMac)
	{
		if (IsPastSparseRenderableLifetime(State.Volume))
		{
			State.SparseActiveBrickCount = 0u;
			State.bSparseActiveBrickCountReadbackPending = false;
			State.SparseActiveBrickCountReadback.Reset();
			State.bSparseAtlasScatterPending = false;
			State.bSparseOccupancyRefreshPending = false;
			return;
		}

		if (State.bSparseAtlasVisibleThisFrame)
		{
			FActiveBrickDispatchResources ActiveBrickResources;
			if (bHasSparseSimulationActiveBricks)
			{
				ActiveBrickResources = SparseSimulationActiveBricks;
			}
			else
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
					DisplacedDensityTextures[State.CurrentDensityIndex],
					VelocityTextures[State.CurrentVelocityIndex],
					BulletCutoutTextures[State.CurrentBulletFieldIndex],
					BulletSinkTextures[State.CurrentBulletFieldIndex],
					GetAdvectionEventBuffer(),
					SparseMaskEventCount,
					BrickActivityTexture,
					bScatterBulletChannels);
				ActiveBrickResources = AddExpandBrickOccupancyPass(
					GraphBuilder,
					State,
					BrickActivityTexture,
					BrickOccupancyTexture,
					static_cast<uint32>(FMath::Max(State.AllocatedSparseAtlasBrickCapacity, 1)),
					0u);
			}
			QueueSparseActiveBrickCountReadback(GraphBuilder, State, ActiveBrickResources.ActiveBrickCountBuffer);
			AddScatterSparseAtlasPass(
				GraphBuilder,
				State,
				DensityTextures[State.CurrentDensityIndex],
				DisplacedDensityTextures[State.CurrentDensityIndex],
				BulletCutoutTextures[State.CurrentBulletFieldIndex],
				BulletSinkTextures[State.CurrentBulletFieldIndex],
				ActiveBrickResources,
				bScatterBulletChannels);
			State.bSparseAtlasScatterPending = false;
			State.bSparseOccupancyRefreshPending = false;
		}
		else
		{
			State.bSparseAtlasScatterPending = true;
			State.bSparseOccupancyRefreshPending = true;
		}
	}
}

void FTimeThiefSmokeViewExtension::AddInitPass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGTextureRef DensityTexture,
	FRDGTextureRef DisplacedDensityTexture,
	FRDGTextureRef VelocityTexture)
{
	const FIntVector GridSize = State.AllocatedGridSize;
	const FIntVector GroupCount = MakeGroupCount(GridSize);

	TShaderMapRef<FTimeThiefSmokeInitCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokeInitCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeInitCS::FParameters>();
	PassParameters->OutDensity = GraphBuilder.CreateUAV(DensityTexture);
	PassParameters->OutDisplacedDensity = GraphBuilder.CreateUAV(DisplacedDensityTexture);
	PassParameters->OutVelocity = GraphBuilder.CreateUAV(VelocityTexture);
	PassParameters->GridResolution = GridSize;
	PassParameters->BoundsExtent = FVector3f(State.Volume.BoundsExtent);
	PassParameters->NaturalBoundsExtent = FVector3f(State.Volume.NaturalBoundsExtent);
	PassParameters->InitialDensity = TimeThiefSmokeParameterDefaults::InitialDensity;
	PassParameters->PlumeSourceRadius = TimeThiefSmokeParameterDefaults::PlumeSourceRadius;

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
	FRDGTextureRef DisplacedDensityIn,
	FRDGTextureRef VelocityIn,
	FRDGTextureRef DensityOut,
	FRDGTextureRef DisplacedDensityOut,
	FRDGTextureRef VelocityOut,
	FRDGBufferRef EventBuffer,
	FRDGBufferRef EventBrickMasksBuffer,
	int32 EventCount,
	float DeltaSeconds,
	const FActiveBrickDispatchResources* ActiveBrickResources)
{
	const FIntVector GridSize = State.AllocatedGridSize;
	const FIntVector GroupCount = MakeGroupCount(GridSize);
	const bool bUseActiveBrickDispatch = ActiveBrickResources &&
		ActiveBrickResources->ActiveBrickCountBuffer &&
		ActiveBrickResources->ActiveBricksBuffer &&
		State.bUseSparseSimulationMaskThisFrame;
	FRDGBufferRef ActiveBrickCountBuffer = ActiveBrickResources ? ActiveBrickResources->ActiveBrickCountBuffer : nullptr;
	FRDGBufferRef ActiveBricksBuffer = ActiveBrickResources ? ActiveBrickResources->ActiveBricksBuffer : nullptr;
	if (!ActiveBrickCountBuffer || !ActiveBricksBuffer)
	{
		CreateEmptyActiveBrickBuffers(GraphBuilder, ActiveBrickCountBuffer, ActiveBricksBuffer);
	}

	TShaderMapRef<FTimeThiefSmokeApplyEventsCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokeApplyEventsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeApplyEventsCS::FParameters>();
	PassParameters->DensityIn = DensityIn;
	PassParameters->DisplacedDensityIn = DisplacedDensityIn;
	PassParameters->VelocityIn = VelocityIn;
	TIME_THIEF_SMOKE_SET_OBSTACLE_FIELD_PARAMETERS(PassParameters, GraphBuilder.RegisterExternalTexture(State.ObstacleSdfTexture), GraphBuilder, State);
	PassParameters->BrickOccupancyTexture = GraphBuilder.RegisterExternalTexture(State.BrickOccupancyTexture);
	PassParameters->VolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->Events = GraphBuilder.CreateSRV(EventBuffer);
	PassParameters->EventBrickMasks = GraphBuilder.CreateSRV(EventBrickMasksBuffer);
	PassParameters->OutDensity = GraphBuilder.CreateUAV(DensityOut);
	PassParameters->OutDisplacedDensity = GraphBuilder.CreateUAV(DisplacedDensityOut);
	PassParameters->OutVelocity = GraphBuilder.CreateUAV(VelocityOut);
	PassParameters->GridResolution = GridSize;
	PassParameters->BrickGridResolution = State.AllocatedBrickGridSize;
	PassParameters->SmokeBrickSize = FMath::Clamp(TimeThiefSmokeParameterDefaults::SmokeBrickSize, TimeThiefSmokeParameterDefaults::SmokeBrickMinSize, TimeThiefSmokeParameterDefaults::SmokeBrickMaxSize);
	PassParameters->MaxActiveSmokeBricks = static_cast<int32>(GetBrickGridCount(State.AllocatedBrickGridSize));
	PassParameters->ActiveBrickCountBuffer = GraphBuilder.CreateSRV(ActiveBrickCountBuffer);
	PassParameters->ActiveBricks = GraphBuilder.CreateSRV(ActiveBricksBuffer);
	PassParameters->bUseSparseSimulationMask = State.bUseSparseSimulationMaskThisFrame ? 1u : 0u;
	PassParameters->bUseActiveBrickDispatch = bUseActiveBrickDispatch ? 1u : 0u;
	PassParameters->bUseEventBrickBins = EventCount > 0 ? 1u : 0u;
	PassParameters->BoundsExtent = FVector3f(State.Volume.BoundsExtent);
	PassParameters->DeltaSeconds = DeltaSeconds;
	PassParameters->SimulationEventDeltaSecondsMax = TimeThiefSmokeParameterDefaults::SimulationEventDeltaSecondsMax;
	PassParameters->SmokeDensityMax = TimeThiefSmokeParameterDefaults::SmokeDensityMax;
	PassParameters->EventCount = EventCount;
	PassParameters->MaxShaderEventCount = TimeThiefSmokeParameterDefaults::ShaderEventLoopMaxCount;
	PassParameters->LocalToWorld = State.Volume.LocalToWorld.ToMatrixWithScale();
	PassParameters->WorldToLocal = State.Volume.LocalToWorld.ToInverseMatrixWithScale();

	if (bUseActiveBrickDispatch)
	{
		AddClearUAVPass(GraphBuilder, PassParameters->OutDensity, 0.0f);
		AddClearUAVPass(GraphBuilder, PassParameters->OutDisplacedDensity, 0.0f);
		AddClearUAVPass(GraphBuilder, PassParameters->OutVelocity, 0.0f);
		FRDGBufferRef IndirectArgsBuffer = ActiveBrickResources->DispatchArgsBuffer
			? ActiveBrickResources->DispatchArgsBuffer
			: AddBuildSparseBrickDispatchArgsPass(
				GraphBuilder,
				State,
				*ActiveBrickResources,
				ComputeSparseScatterGroupsPerBrick(PassParameters->SmokeBrickSize),
				GetBrickGridCount(State.AllocatedBrickGridSize));
		PassParameters->SparseDispatchIndirectArgsBuffer = IndirectArgsBuffer;
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("TimeThiefSmoke.ApplyEventsActive SmokeId=%d Events=%d", State.Volume.SmokeId, EventCount),
			ComputeShader,
			PassParameters,
			IndirectArgsBuffer,
			0);
		return;
	}

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
	FRDGTextureRef DisplacedDensityIn,
	FRDGTextureRef VelocityIn,
	FRDGTextureRef DensityOut,
	FRDGTextureRef DisplacedDensityOut,
	FRDGTextureRef VelocityOut,
	FRDGBufferRef EventBuffer,
	FRDGBufferRef EventBrickMasksBuffer,
	int32 EventCount,
	float DeltaSeconds,
	const FActiveBrickDispatchResources* ActiveBrickResources)
{
	const FIntVector GridSize = State.AllocatedGridSize;
	const FIntVector GroupCount = MakeGroupCount(GridSize);
	const bool bUseActiveBrickDispatch = ActiveBrickResources &&
		ActiveBrickResources->ActiveBrickCountBuffer &&
		ActiveBrickResources->ActiveBricksBuffer &&
		State.bUseSparseSimulationMaskThisFrame;
	FRDGBufferRef ActiveBrickCountBuffer = ActiveBrickResources ? ActiveBrickResources->ActiveBrickCountBuffer : nullptr;
	FRDGBufferRef ActiveBricksBuffer = ActiveBrickResources ? ActiveBrickResources->ActiveBricksBuffer : nullptr;
	if (!ActiveBrickCountBuffer || !ActiveBricksBuffer)
	{
		CreateEmptyActiveBrickBuffers(GraphBuilder, ActiveBrickCountBuffer, ActiveBricksBuffer);
	}

	TShaderMapRef<FTimeThiefSmokeDynamicObstacleCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokeDynamicObstacleCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeDynamicObstacleCS::FParameters>();
	PassParameters->DensityIn = DensityIn;
	PassParameters->DisplacedDensityIn = DisplacedDensityIn;
	PassParameters->VelocityIn = VelocityIn;
	TIME_THIEF_SMOKE_SET_OBSTACLE_FIELD_PARAMETERS(PassParameters, GraphBuilder.RegisterExternalTexture(State.ObstacleSdfTexture), GraphBuilder, State);
	PassParameters->BrickOccupancyTexture = GraphBuilder.RegisterExternalTexture(State.BrickOccupancyTexture);
	PassParameters->VolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->Events = GraphBuilder.CreateSRV(EventBuffer);
	PassParameters->EventBrickMasks = GraphBuilder.CreateSRV(EventBrickMasksBuffer);
	PassParameters->OutDensity = GraphBuilder.CreateUAV(DensityOut);
	PassParameters->OutDisplacedDensity = GraphBuilder.CreateUAV(DisplacedDensityOut);
	PassParameters->OutVelocity = GraphBuilder.CreateUAV(VelocityOut);
	PassParameters->GridResolution = GridSize;
	PassParameters->BrickGridResolution = State.AllocatedBrickGridSize;
	PassParameters->SmokeBrickSize = FMath::Clamp(TimeThiefSmokeParameterDefaults::SmokeBrickSize, TimeThiefSmokeParameterDefaults::SmokeBrickMinSize, TimeThiefSmokeParameterDefaults::SmokeBrickMaxSize);
	PassParameters->MaxActiveSmokeBricks = static_cast<int32>(GetBrickGridCount(State.AllocatedBrickGridSize));
	PassParameters->ActiveBrickCountBuffer = GraphBuilder.CreateSRV(ActiveBrickCountBuffer);
	PassParameters->ActiveBricks = GraphBuilder.CreateSRV(ActiveBricksBuffer);
	PassParameters->bUseSparseSimulationMask = State.bUseSparseSimulationMaskThisFrame ? 1u : 0u;
	PassParameters->bUseActiveBrickDispatch = bUseActiveBrickDispatch ? 1u : 0u;
	PassParameters->bUseEventBrickBins = EventCount > 0 ? 1u : 0u;
	PassParameters->BoundsExtent = FVector3f(State.Volume.BoundsExtent);
	PassParameters->DeltaSeconds = DeltaSeconds;
	PassParameters->SimulationEventDeltaSecondsMax = TimeThiefSmokeParameterDefaults::SimulationEventDeltaSecondsMax;
	PassParameters->SmokeDensityMax = TimeThiefSmokeParameterDefaults::SmokeDensityMax;
	PassParameters->ActorAirflowStrength = FMath::Max(0.0f, TimeThiefSmokeParameterDefaults::ActorAirflowStrength);
	PassParameters->ActorAirflowMinSpeed = FMath::Max(0.0f, TimeThiefSmokeParameterDefaults::ActorAirflowMinSpeed);
	PassParameters->ActorAirflowFullSpeed = FMath::Max(PassParameters->ActorAirflowMinSpeed + TimeThiefSmokeParameterDefaults::ActorAirflowFullSpeedMinGap, TimeThiefSmokeParameterDefaults::ActorAirflowFullSpeed);
	PassParameters->ActorAirflowRadiusScale = FMath::Max(TimeThiefSmokeParameterDefaults::ActorAirflowRadiusScaleMin, TimeThiefSmokeParameterDefaults::ActorAirflowRadiusScale);
	PassParameters->ActorAirflowFrontStrength = FMath::Max(0.0f, TimeThiefSmokeParameterDefaults::ActorAirflowFrontStrength);
	PassParameters->ActorAirflowSideStrength = FMath::Max(0.0f, TimeThiefSmokeParameterDefaults::ActorAirflowSideStrength);
	PassParameters->ActorAirflowWakeStrength = FMath::Max(0.0f, TimeThiefSmokeParameterDefaults::ActorAirflowWakeStrength);
	PassParameters->ActorWakeTrailLengthScale = TimeThiefSmokeParameterDefaults::ActorWakeTrailLengthScale;
	PassParameters->ActorWakeStreetLaneInnerRadiusScale = TimeThiefSmokeParameterDefaults::ActorWakeStreetLaneInnerRadiusScale;
	PassParameters->EventCount = EventCount;
	PassParameters->MaxShaderEventCount = TimeThiefSmokeParameterDefaults::ShaderEventLoopMaxCount;
	PassParameters->LocalToWorld = State.Volume.LocalToWorld.ToMatrixWithScale();
	PassParameters->WorldToLocal = State.Volume.LocalToWorld.ToInverseMatrixWithScale();

	if (bUseActiveBrickDispatch)
	{
		AddClearUAVPass(GraphBuilder, PassParameters->OutDensity, 0.0f);
		AddClearUAVPass(GraphBuilder, PassParameters->OutDisplacedDensity, 0.0f);
		AddClearUAVPass(GraphBuilder, PassParameters->OutVelocity, 0.0f);
		FRDGBufferRef IndirectArgsBuffer = ActiveBrickResources->DispatchArgsBuffer
			? ActiveBrickResources->DispatchArgsBuffer
			: AddBuildSparseBrickDispatchArgsPass(
				GraphBuilder,
				State,
				*ActiveBrickResources,
				ComputeSparseScatterGroupsPerBrick(PassParameters->SmokeBrickSize),
				GetBrickGridCount(State.AllocatedBrickGridSize));
		PassParameters->SparseDispatchIndirectArgsBuffer = IndirectArgsBuffer;
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("TimeThiefSmoke.DynamicObstacleActive SmokeId=%d Events=%d", State.Volume.SmokeId, EventCount),
			ComputeShader,
			PassParameters,
			IndirectArgsBuffer,
			0);
		return;
	}

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
	FRDGTextureRef DisplacedDensityIn,
	FRDGTextureRef VelocityIn,
	FRDGTextureRef BulletCutoutTexture,
	FRDGTextureRef BulletSinkTexture,
	FRDGBufferRef EventBuffer,
	FRDGBufferRef EventBrickMasksBuffer,
	int32 EventCount,
	FRDGTextureRef DensityOut,
	FRDGTextureRef DisplacedDensityOut,
	FRDGTextureRef VelocityOut,
	FRDGTextureRef BulletCutoutOut,
	FRDGTextureRef BulletSinkOut,
	float DeltaSeconds,
	const FActiveBrickDispatchResources* ActiveBrickResources)
{
	const FIntVector GridSize = State.AllocatedGridSize;
	const FIntVector GroupCount = MakeGroupCount(GridSize);
	const bool bUseActiveBrickDispatch = ActiveBrickResources &&
		ActiveBrickResources->ActiveBrickCountBuffer &&
		ActiveBrickResources->ActiveBricksBuffer &&
		State.bUseSparseSimulationMaskThisFrame;
	FRDGBufferRef ActiveBrickCountBuffer = ActiveBrickResources ? ActiveBrickResources->ActiveBrickCountBuffer : nullptr;
	FRDGBufferRef ActiveBricksBuffer = ActiveBrickResources ? ActiveBrickResources->ActiveBricksBuffer : nullptr;
	if (!ActiveBrickCountBuffer || !ActiveBricksBuffer)
	{
		CreateEmptyActiveBrickBuffers(GraphBuilder, ActiveBrickCountBuffer, ActiveBricksBuffer);
	}
	const bool bHasBulletFields = BulletCutoutTexture && BulletSinkTexture && BulletCutoutOut && BulletSinkOut;
	if (!BulletCutoutTexture || !BulletSinkTexture)
	{
		FRDGTextureRef ZeroBulletTexture = GSystemTextures.GetDefaultTexture(GraphBuilder, ETextureDimension::Texture3D, PF_R16F, FClearValueBinding::Black);
		BulletCutoutTexture = BulletCutoutTexture ? BulletCutoutTexture : ZeroBulletTexture;
		BulletSinkTexture = BulletSinkTexture ? BulletSinkTexture : ZeroBulletTexture;
	}
	if (!BulletCutoutOut || !BulletSinkOut)
	{
		const FRDGTextureDesc DummyBulletDesc = FRDGTextureDesc::Create3D(
			FIntVector(1, 1, 1),
			PF_R16F,
			FClearValueBinding::Black,
			TexCreate_ShaderResource | TexCreate_UAV);
		BulletCutoutOut = BulletCutoutOut ? BulletCutoutOut : GraphBuilder.CreateTexture(DummyBulletDesc, TEXT("TimeThiefSmoke.DummyBulletCutout"));
		BulletSinkOut = BulletSinkOut ? BulletSinkOut : GraphBuilder.CreateTexture(DummyBulletDesc, TEXT("TimeThiefSmoke.DummyBulletSink"));
	}

	const auto SetCommonSimulateParameters = [&](auto* PassParameters)
	{
		PassParameters->DensityIn = DensityIn;
		PassParameters->DisplacedDensityIn = DisplacedDensityIn;
		PassParameters->VelocityIn = VelocityIn;
		TIME_THIEF_SMOKE_SET_OBSTACLE_FIELD_PARAMETERS(PassParameters, GraphBuilder.RegisterExternalTexture(State.ObstacleSdfTexture), GraphBuilder, State);
		PassParameters->BrickOccupancyTexture = GraphBuilder.RegisterExternalTexture(State.BrickOccupancyTexture);
		PassParameters->VolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		PassParameters->Events = GraphBuilder.CreateSRV(EventBuffer);
		PassParameters->EventBrickMasks = GraphBuilder.CreateSRV(EventBrickMasksBuffer);
		PassParameters->OutDensity = GraphBuilder.CreateUAV(DensityOut);
		PassParameters->OutDisplacedDensity = GraphBuilder.CreateUAV(DisplacedDensityOut);
		PassParameters->OutVelocity = GraphBuilder.CreateUAV(VelocityOut);
		PassParameters->GridResolution = GridSize;
		PassParameters->BrickGridResolution = State.AllocatedBrickGridSize;
		PassParameters->SmokeBrickSize = FMath::Clamp(TimeThiefSmokeParameterDefaults::SmokeBrickSize, TimeThiefSmokeParameterDefaults::SmokeBrickMinSize, TimeThiefSmokeParameterDefaults::SmokeBrickMaxSize);
		PassParameters->MaxActiveSmokeBricks = static_cast<int32>(GetBrickGridCount(State.AllocatedBrickGridSize));
		PassParameters->ActiveBrickCountBuffer = GraphBuilder.CreateSRV(ActiveBrickCountBuffer);
		PassParameters->ActiveBricks = GraphBuilder.CreateSRV(ActiveBricksBuffer);
		PassParameters->bUseSparseSimulationMask = State.bUseSparseSimulationMaskThisFrame ? 1u : 0u;
		PassParameters->bUseActiveBrickDispatch = bUseActiveBrickDispatch ? 1u : 0u;
		PassParameters->bUseEventBrickBins = EventCount > 0 ? 1u : 0u;
		PassParameters->bHasBulletFields = bHasBulletFields ? 1u : 0u;
		PassParameters->BoundsExtent = FVector3f(State.Volume.BoundsExtent);
		PassParameters->NaturalBoundsExtent = FVector3f(State.Volume.NaturalBoundsExtent);
		PassParameters->DeltaSeconds = DeltaSeconds;
		PassParameters->InitialDensity = TimeThiefSmokeParameterDefaults::InitialDensity;
		PassParameters->AgeSeconds = State.Volume.AgeSeconds;
		PassParameters->DurationSeconds = State.Volume.DurationSeconds;
		PassParameters->SmokeFadeOutDuration = TimeThiefSmokeParameterDefaults::SmokeFadeOutDuration;
		PassParameters->PlumeEmissionDuration = TimeThiefSmokeParameterDefaults::PlumeEmissionDuration;
		PassParameters->PlumeSourceRadius = TimeThiefSmokeParameterDefaults::PlumeSourceRadius;
		PassParameters->ObstacleSourceClearRadiusScale = TimeThiefSmokeParameterDefaults::ObstacleSourceClearRadiusScale;
		PassParameters->PlumeExpansionVelocity = TimeThiefSmokeParameterDefaults::PlumeExpansionVelocity;
		PassParameters->PlumeRiseVelocity = TimeThiefSmokeParameterDefaults::PlumeRiseVelocity;
		PassParameters->DensityDissipation = TimeThiefSmokeParameterDefaults::DensityDissipation;
		PassParameters->VelocityDamping = TimeThiefSmokeParameterDefaults::VelocityDamping;
		PassParameters->VorticityStrength = TimeThiefSmokeParameterDefaults::VorticityStrength;
		PassParameters->SelfWobbleTimeScale = TimeThiefSmokeParameterDefaults::SelfWobbleTimeScale;
		PassParameters->SelfWobbleVelocityScale = TimeThiefSmokeParameterDefaults::SelfWobbleVelocityScale;
		PassParameters->bUseMacCormackAdvection = TimeThiefSmokeParameterDefaults::bUseMacCormackAdvection ? 1u : 0u;
		PassParameters->bUseAdaptiveMacCormack = TimeThiefSmokeParameterDefaults::bUseAdaptiveMacCormack ? 1u : 0u;
		PassParameters->SmokeDensityMax = TimeThiefSmokeParameterDefaults::SmokeDensityMax;
		PassParameters->EventCount = EventCount;
		PassParameters->MaxShaderEventCount = TimeThiefSmokeParameterDefaults::ShaderEventLoopMaxCount;
		PassParameters->LocalToWorld = State.Volume.LocalToWorld.ToMatrixWithScale();
		PassParameters->WorldToLocal = State.Volume.LocalToWorld.ToInverseMatrixWithScale();
	};

	TShaderMapRef<FTimeThiefSmokeSimulateCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokeSimulateCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeSimulateCS::FParameters>();
	SetCommonSimulateParameters(PassParameters);
	PassParameters->BulletCutoutTexture = BulletCutoutTexture;
	PassParameters->BulletSinkTexture = BulletSinkTexture;
	PassParameters->OutCutout = GraphBuilder.CreateUAV(BulletCutoutOut);
	PassParameters->OutSink = GraphBuilder.CreateUAV(BulletSinkOut);
	PassParameters->BulletWakeCutoutLife = FMath::Max(TimeThiefSmokeParameterDefaults::BulletWakeMinLifeSeconds, TimeThiefSmokeParameterDefaults::BulletWakeMaxVisibleLife);
	PassParameters->BulletWakeReleaseDuration = FMath::Max(TimeThiefSmokeParameterDefaults::BulletWakeMinLifeSeconds, TimeThiefSmokeParameterDefaults::BulletWakeReleaseDuration);
	PassParameters->BulletWakeSinkLife = FMath::Max(TimeThiefSmokeParameterDefaults::BulletWakeMinLifeSeconds, TimeThiefSmokeParameterDefaults::BulletWakeSinkLife);
	PassParameters->BulletWakeSinkStrength = FMath::Max(0.0f, TimeThiefSmokeParameterDefaults::BulletWakeSinkStrength);
	PassParameters->BulletWakeImpulseStrength = TimeThiefSmokeParameterDefaults::BulletWakeImpulseStrength;
	PassParameters->BulletWakeCutoutFeather = FMath::Max(TimeThiefSmokeParameterDefaults::BulletWakeCutoutFeatherMin, TimeThiefSmokeParameterDefaults::BulletWakeCutoutFeather);
	PassParameters->BulletWakeHoldCoreInnerRadiusScale = TimeThiefSmokeParameterDefaults::BulletWakeHoldCoreInnerRadiusScale;
	PassParameters->BulletWakeHoldCoreOuterRadiusScale = TimeThiefSmokeParameterDefaults::BulletWakeHoldCoreOuterRadiusScale;
	if (bUseActiveBrickDispatch)
	{
		AddClearUAVPass(GraphBuilder, PassParameters->OutDensity, 0.0f);
		AddClearUAVPass(GraphBuilder, PassParameters->OutDisplacedDensity, 0.0f);
		AddClearUAVPass(GraphBuilder, PassParameters->OutVelocity, 0.0f);
		if (bHasBulletFields)
		{
			AddClearUAVPass(GraphBuilder, PassParameters->OutCutout, 0.0f);
			AddClearUAVPass(GraphBuilder, PassParameters->OutSink, 0.0f);
		}
		FRDGBufferRef IndirectArgsBuffer = ActiveBrickResources->DispatchArgsBuffer
			? ActiveBrickResources->DispatchArgsBuffer
			: AddBuildSparseBrickDispatchArgsPass(
				GraphBuilder,
				State,
				*ActiveBrickResources,
				ComputeSparseScatterGroupsPerBrick(PassParameters->SmokeBrickSize),
				GetBrickGridCount(State.AllocatedBrickGridSize));
		PassParameters->SparseDispatchIndirectArgsBuffer = IndirectArgsBuffer;
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("TimeThiefSmoke.SimulateActive SmokeId=%d", State.Volume.SmokeId),
			ComputeShader,
			PassParameters,
			IndirectArgsBuffer,
			0);
		return;
	}

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.Simulate SmokeId=%d", State.Volume.SmokeId),
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
	TIME_THIEF_SMOKE_SET_OBSTACLE_FIELD_PARAMETERS(PassParameters, GraphBuilder.RegisterExternalTexture(State.ObstacleSdfTexture), GraphBuilder, State);
	PassParameters->BrickOccupancyTexture = GraphBuilder.RegisterExternalTexture(State.BrickOccupancyTexture);
	PassParameters->VolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->OutCurl = GraphBuilder.CreateUAV(CurlOut);
	PassParameters->BrickGridResolution = State.AllocatedBrickGridSize;
	PassParameters->SmokeBrickSize = FMath::Clamp(TimeThiefSmokeParameterDefaults::SmokeBrickSize, TimeThiefSmokeParameterDefaults::SmokeBrickMinSize, TimeThiefSmokeParameterDefaults::SmokeBrickMaxSize);
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
	FRDGTextureRef DisplacedDensityIn,
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
	PassParameters->DisplacedDensityIn = DisplacedDensityIn;
	PassParameters->VelocityIn = VelocityIn;
	PassParameters->CurlTexture = CurlTexture;
	TIME_THIEF_SMOKE_SET_OBSTACLE_FIELD_PARAMETERS(PassParameters, GraphBuilder.RegisterExternalTexture(State.ObstacleSdfTexture), GraphBuilder, State);
	PassParameters->BrickOccupancyTexture = GraphBuilder.RegisterExternalTexture(State.BrickOccupancyTexture);
	PassParameters->VolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->Events = GraphBuilder.CreateSRV(EventBuffer);
	PassParameters->EventBrickMasks = GraphBuilder.CreateSRV(EventBrickMasksBuffer);
	PassParameters->OutVelocity = GraphBuilder.CreateUAV(VelocityOut);
	PassParameters->GridResolution = GridSize;
	PassParameters->BrickGridResolution = State.AllocatedBrickGridSize;
	PassParameters->SmokeBrickSize = FMath::Clamp(TimeThiefSmokeParameterDefaults::SmokeBrickSize, TimeThiefSmokeParameterDefaults::SmokeBrickMinSize, TimeThiefSmokeParameterDefaults::SmokeBrickMaxSize);
	PassParameters->bUseSparseSimulationMask = State.bUseSparseSimulationMaskThisFrame ? 1u : 0u;
	PassParameters->bUseEventBrickBins = EventCount > 0 ? 1u : 0u;
	PassParameters->BoundsExtent = FVector3f(State.Volume.BoundsExtent);
	PassParameters->CellSize = CellSize;
	PassParameters->DeltaSeconds = DeltaSeconds;
	PassParameters->AgeSeconds = State.Volume.AgeSeconds;
	PassParameters->VorticityConfinementStrength = TimeThiefSmokeParameterDefaults::VorticityConfinementStrength;
	PassParameters->TurbulenceStrength = TimeThiefSmokeParameterDefaults::TurbulenceStrength;
	PassParameters->AirInteractionStrength = TimeThiefSmokeParameterDefaults::AirInteractionStrength;
	PassParameters->SelfWobbleTimeScale = TimeThiefSmokeParameterDefaults::SelfWobbleTimeScale;
	PassParameters->SelfWobbleForceScale = TimeThiefSmokeParameterDefaults::SelfWobbleForceScale;
	PassParameters->EventVortexStrength = TimeThiefSmokeParameterDefaults::EventVortexStrength;
	PassParameters->MaxSmokeVelocity = TimeThiefSmokeParameterDefaults::MaxSmokeVelocity;
	PassParameters->SimulationEventDeltaSecondsMax = TimeThiefSmokeParameterDefaults::SimulationEventDeltaSecondsMax;
	PassParameters->ActorAirflowFullSpeed = FMath::Max(TimeThiefSmokeParameterDefaults::ActorAirflowFullSpeedMinGap, TimeThiefSmokeParameterDefaults::ActorAirflowFullSpeed);
	PassParameters->ActorAirflowRadiusScale = FMath::Max(TimeThiefSmokeParameterDefaults::ActorAirflowRadiusScaleMin, TimeThiefSmokeParameterDefaults::ActorAirflowRadiusScale);
	PassParameters->ActorAirflowVortexStrength = FMath::Max(0.0f, TimeThiefSmokeParameterDefaults::ActorAirflowVortexStrength);
	PassParameters->ActorWakeSurfaceRollForce = TimeThiefSmokeParameterDefaults::ActorWakeSurfaceRollForce;
	PassParameters->ActorWakeSurfaceTangentSpeedScale = TimeThiefSmokeParameterDefaults::ActorWakeSurfaceTangentSpeedScale;
	PassParameters->ActorWakeSurfaceNoiseForce = TimeThiefSmokeParameterDefaults::ActorWakeSurfaceNoiseForce;
	PassParameters->ActorWakeTrailMinRollForce = TimeThiefSmokeParameterDefaults::ActorWakeTrailMinRollForce;
	PassParameters->ActorWakeTrailMaxRollForce = TimeThiefSmokeParameterDefaults::ActorWakeTrailMaxRollForce;
	PassParameters->ActorWakeStreetForceScale = TimeThiefSmokeParameterDefaults::ActorWakeStreetForceScale;
	PassParameters->ActorWakeFrontPushScale = TimeThiefSmokeParameterDefaults::ActorWakeFrontPushScale;
	PassParameters->ActorWakeStreetLaneInnerRadiusScale = TimeThiefSmokeParameterDefaults::ActorWakeStreetLaneInnerRadiusScale;
	PassParameters->AirInteractionRollBaseForce = TimeThiefSmokeParameterDefaults::AirInteractionRollBaseForce;
	PassParameters->AirInteractionTangentialSpeedScale = TimeThiefSmokeParameterDefaults::AirInteractionTangentialSpeedScale;
	PassParameters->AirInteractionCurlNoiseForce = TimeThiefSmokeParameterDefaults::AirInteractionCurlNoiseForce;
	PassParameters->VorticityConfinementForceScale = TimeThiefSmokeParameterDefaults::VorticityConfinementForceScale;
	PassParameters->TurbulenceBandMinForce = TimeThiefSmokeParameterDefaults::TurbulenceBandMinForce;
	PassParameters->TurbulenceBandMaxForce = TimeThiefSmokeParameterDefaults::TurbulenceBandMaxForce;
	PassParameters->TurbulenceCurlMagnitudeScale = TimeThiefSmokeParameterDefaults::TurbulenceCurlMagnitudeScale;
	PassParameters->AmbientAirCurlForce = TimeThiefSmokeParameterDefaults::AmbientAirCurlForce;
	PassParameters->VorticityDeltaSpeedMin = TimeThiefSmokeParameterDefaults::VorticityDeltaSpeedMin;
	PassParameters->VorticityDeltaSpeedStrengthScale = TimeThiefSmokeParameterDefaults::VorticityDeltaSpeedStrengthScale;
	PassParameters->ActorWakeTrailLengthScale = TimeThiefSmokeParameterDefaults::ActorWakeTrailLengthScale;
	PassParameters->BulletWakeCutoutFeather = FMath::Max(TimeThiefSmokeParameterDefaults::BulletWakeCutoutFeatherMin, TimeThiefSmokeParameterDefaults::BulletWakeCutoutFeather);
	PassParameters->EventCount = EventCount;
	PassParameters->MaxShaderEventCount = TimeThiefSmokeParameterDefaults::ShaderEventLoopMaxCount;
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
	TIME_THIEF_SMOKE_SET_OBSTACLE_FIELD_PARAMETERS(PassParameters, GraphBuilder.RegisterExternalTexture(State.ObstacleSdfTexture), GraphBuilder, State);
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
	FRDGTextureRef DisplacedDensityTexture,
	FRDGTextureRef VelocityTexture,
	FRDGTextureRef BulletCutoutTexture,
	FRDGTextureRef BulletSinkTexture,
	FRDGBufferRef EventBuffer,
	int32 EventCount,
	FRDGTextureRef BrickActivityTexture,
	bool bCheckBulletChannels)
{
	const int32 BrickSize = FMath::Clamp(TimeThiefSmokeParameterDefaults::SmokeBrickSize, TimeThiefSmokeParameterDefaults::SmokeBrickMinSize, TimeThiefSmokeParameterDefaults::SmokeBrickMaxSize);
	const FIntVector BrickGridSize = MakeBrickGridSize(State.AllocatedGridSize, BrickSize);
	const FIntVector GroupCount = BrickGridSize;

	TShaderMapRef<FTimeThiefSmokeBuildBrickOccupancyCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokeBuildBrickOccupancyCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeBuildBrickOccupancyCS::FParameters>();
	PassParameters->GridResolution = State.AllocatedGridSize;
	PassParameters->BrickGridResolution = BrickGridSize;
	PassParameters->SmokeBrickSize = BrickSize;
	PassParameters->BoundsExtent = FVector3f(State.Volume.BoundsExtent);
	PassParameters->DensityTexture = DensityTexture;
	PassParameters->DisplacedDensityTexture = DisplacedDensityTexture;
	PassParameters->VelocityIn = VelocityTexture;
	PassParameters->BulletCutoutTexture = BulletCutoutTexture;
	PassParameters->BulletSinkTexture = BulletSinkTexture;
	PassParameters->bCheckBulletChannels = bCheckBulletChannels ? 1u : 0u;
	PassParameters->EventCount = EventCount;
	PassParameters->MaxShaderEventCount = TimeThiefSmokeParameterDefaults::ShaderEventLoopMaxCount;
	PassParameters->LocalToWorld = State.Volume.LocalToWorld.ToMatrixWithScale();
	PassParameters->Events = GraphBuilder.CreateSRV(EventBuffer);
	PassParameters->ActorAirflowMinSpeed = FMath::Max(0.0f, TimeThiefSmokeParameterDefaults::ActorAirflowMinSpeed);
	PassParameters->ActorAirflowFullSpeed = FMath::Max(PassParameters->ActorAirflowMinSpeed + TimeThiefSmokeParameterDefaults::ActorAirflowFullSpeedMinGap, TimeThiefSmokeParameterDefaults::ActorAirflowFullSpeed);
	PassParameters->ActorAirflowRadiusScale = FMath::Max(TimeThiefSmokeParameterDefaults::ActorAirflowRadiusScaleMin, TimeThiefSmokeParameterDefaults::ActorAirflowRadiusScale);
	PassParameters->ActorWakeTrailLengthScale = TimeThiefSmokeParameterDefaults::ActorWakeTrailLengthScale;
	PassParameters->BulletWakeCutoutFeather = FMath::Max(TimeThiefSmokeParameterDefaults::BulletWakeCutoutFeatherMin, TimeThiefSmokeParameterDefaults::BulletWakeCutoutFeather);
	PassParameters->SparseVelocityActiveThreshold = FMath::Max(0.0f, TimeThiefSmokeParameterDefaults::SparseVelocityActiveThreshold);
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
	uint32 MaxActiveListBricks,
	uint32 MaxDispatchBrickCount)
{
	FActiveBrickDispatchResources Resources;
	const FIntVector BrickGridSize = State.AllocatedBrickGridSize;
	const FIntVector GroupCount = MakeGroupCount(BrickGridSize);
	const int32 MaxActiveSmokeBricks = FMath::Max(State.AllocatedSparseAtlasBrickCapacity, 1);
	const uint32 ActiveListCapacity = FMath::Max(MaxActiveListBricks, 1u);

	TShaderMapRef<FTimeThiefSmokeExpandBrickOccupancyCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FRDGBufferRef BrickAllocatorBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1),
		TEXT("TimeThiefSmoke.BrickAllocator"));
	FRDGBufferUAVRef BrickAllocatorUAV = GraphBuilder.CreateUAV(BrickAllocatorBuffer);
	AddClearUAVPass(GraphBuilder, BrickAllocatorUAV, 0u);
	FRDGBufferRef ActiveBricksBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32) * 4, ActiveListCapacity),
		TEXT("TimeThiefSmoke.ActiveBricks"));

	FTimeThiefSmokeExpandBrickOccupancyCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeExpandBrickOccupancyCS::FParameters>();
	PassParameters->BrickGridResolution = BrickGridSize;
	PassParameters->MaxActiveSmokeBricks = MaxActiveSmokeBricks;
	PassParameters->MaxActiveListBricks = static_cast<int32>(ActiveListCapacity);
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

	Resources.ActiveBrickCountBuffer = BrickAllocatorBuffer;
	Resources.ActiveBricksBuffer = ActiveBricksBuffer;
	if (MaxDispatchBrickCount > 0u)
	{
		Resources.DispatchArgsBuffer = AddBuildSparseBrickDispatchArgsPass(
			GraphBuilder,
			State,
			Resources,
			ComputeSparseScatterGroupsPerBrick(FMath::Clamp(TimeThiefSmokeParameterDefaults::SmokeBrickSize, TimeThiefSmokeParameterDefaults::SmokeBrickMinSize, TimeThiefSmokeParameterDefaults::SmokeBrickMaxSize)),
			MaxDispatchBrickCount);
	}
	return Resources;
}

FTimeThiefSmokeViewExtension::FActiveBrickDispatchResources FTimeThiefSmokeViewExtension::AddBuildActiveBrickListPass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGTextureRef BrickOccupancyTexture)
{
	const FIntVector BrickGridSize = State.AllocatedBrickGridSize;
	const uint32 BrickCount = GetBrickGridCount(BrickGridSize);
	FRDGBufferRef BrickAllocatorBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1),
		TEXT("TimeThiefSmoke.SimulationActiveBrickAllocator"));
	FRDGBufferUAVRef BrickAllocatorUAV = GraphBuilder.CreateUAV(BrickAllocatorBuffer);
	AddClearUAVPass(GraphBuilder, BrickAllocatorUAV, 0u);
	FRDGBufferRef ActiveBricksBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32) * 4, BrickCount),
		TEXT("TimeThiefSmoke.SimulationActiveBricks"));

	TShaderMapRef<FTimeThiefSmokeBuildActiveBrickListCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokeBuildActiveBrickListCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeBuildActiveBrickListCS::FParameters>();
	PassParameters->BrickGridResolution = BrickGridSize;
	PassParameters->MaxActiveSmokeBricks = static_cast<int32>(BrickCount);
	PassParameters->BrickOccupancyTexture = BrickOccupancyTexture;
	PassParameters->BrickAllocator = BrickAllocatorUAV;
	PassParameters->OutActiveBricks = GraphBuilder.CreateUAV(ActiveBricksBuffer);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.BuildActiveBrickList SmokeId=%d", State.Volume.SmokeId),
		ComputeShader,
		PassParameters,
		MakeGroupCount(BrickGridSize));

	FActiveBrickDispatchResources Resources;
	Resources.ActiveBrickCountBuffer = BrickAllocatorBuffer;
	Resources.ActiveBricksBuffer = ActiveBricksBuffer;
	Resources.DispatchArgsBuffer = AddBuildSparseBrickDispatchArgsPass(
		GraphBuilder,
		State,
		Resources,
		ComputeSparseScatterGroupsPerBrick(FMath::Clamp(TimeThiefSmokeParameterDefaults::SmokeBrickSize, TimeThiefSmokeParameterDefaults::SmokeBrickMinSize, TimeThiefSmokeParameterDefaults::SmokeBrickMaxSize)),
		BrickCount);
	return Resources;
}

FRDGBufferRef FTimeThiefSmokeViewExtension::AddBuildSparseBrickDispatchArgsPass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	const FActiveBrickDispatchResources& ActiveBrickResources,
	uint32 GroupsPerBrick,
	uint32 MaxDispatchBrickCount)
{
	FRDGBufferRef IndirectArgsBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(),
		TEXT("TimeThiefSmoke.SparseBrickDispatchArgs"));
	TShaderMapRef<FTimeThiefSmokeBuildSparseScatterArgsCS> BuildArgsShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokeBuildSparseScatterArgsCS::FParameters* BuildArgsParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeBuildSparseScatterArgsCS::FParameters>();
	BuildArgsParameters->MaxActiveSmokeBricks = static_cast<int32>(FMath::Max(MaxDispatchBrickCount, 1u));
	BuildArgsParameters->SparseScatterGroupsPerBrick = FMath::Max(GroupsPerBrick, 1u);
	BuildArgsParameters->ActiveBrickCountBuffer = GraphBuilder.CreateSRV(ActiveBrickResources.ActiveBrickCountBuffer);
	BuildArgsParameters->OutSparseScatterIndirectArgs = GraphBuilder.CreateUAV(IndirectArgsBuffer, PF_R32_UINT);
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.BuildSparseBrickDispatchArgs SmokeId=%d", State.Volume.SmokeId),
		BuildArgsShader,
		BuildArgsParameters,
		FIntVector(1, 1, 1));
	return IndirectArgsBuffer;
}

void FTimeThiefSmokeViewExtension::AddScatterSparseAtlasPass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGTextureRef DensityTexture,
	FRDGTextureRef DisplacedDensityTexture,
	FRDGTextureRef BulletCutoutTexture,
	FRDGTextureRef BulletSinkTexture,
	const FActiveBrickDispatchResources& ActiveBrickResources,
	bool bScatterBulletChannels)
{
	const int32 BrickSize = FMath::Clamp(TimeThiefSmokeParameterDefaults::SmokeBrickSize, TimeThiefSmokeParameterDefaults::SmokeBrickMinSize, TimeThiefSmokeParameterDefaults::SmokeBrickMaxSize);
	const FIntVector GridSize = State.AllocatedGridSize;
	if (!ActiveBrickResources.ActiveBrickCountBuffer ||
		!ActiveBrickResources.ActiveBricksBuffer ||
		!State.SparseFieldAtlasTexture.IsValid() ||
		FMath::Min3(State.AllocatedSparseAtlasBrickGridSize.X, State.AllocatedSparseAtlasBrickGridSize.Y, State.AllocatedSparseAtlasBrickGridSize.Z) <= 0 ||
		FMath::Min3(State.AllocatedSparseAtlasGridSize.X, State.AllocatedSparseAtlasGridSize.Y, State.AllocatedSparseAtlasGridSize.Z) <= 0)
	{
		return;
	}
	const uint32 SparseScatterGroupsPerBrick = ComputeSparseScatterGroupsPerBrick(BrickSize);
	FRDGBufferRef IndirectArgsBuffer = AddBuildSparseBrickDispatchArgsPass(
		GraphBuilder,
		State,
		ActiveBrickResources,
		SparseScatterGroupsPerBrick,
		static_cast<uint32>(FMath::Max(State.AllocatedSparseAtlasBrickCapacity, 1)));

	TShaderMapRef<FTimeThiefSmokeScatterSparseAtlasCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokeScatterSparseAtlasCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeScatterSparseAtlasCS::FParameters>();
	PassParameters->GridResolution = GridSize;
	PassParameters->BrickGridResolution = State.AllocatedBrickGridSize;
	PassParameters->SparseAtlasBrickGridResolution = State.AllocatedSparseAtlasBrickGridSize;
	PassParameters->SmokeBrickSize = BrickSize;
	PassParameters->MaxActiveSmokeBricks = FMath::Max(State.AllocatedSparseAtlasBrickCapacity, 1);
	PassParameters->ActiveBrickCountBuffer = GraphBuilder.CreateSRV(ActiveBrickResources.ActiveBrickCountBuffer);
	PassParameters->ActiveBricks = GraphBuilder.CreateSRV(ActiveBrickResources.ActiveBricksBuffer);
	PassParameters->DensityTexture = DensityTexture;
	PassParameters->DisplacedDensityTexture = DisplacedDensityTexture;
	PassParameters->BulletCutoutTexture = BulletCutoutTexture;
	PassParameters->BulletSinkTexture = BulletSinkTexture;
	PassParameters->bScatterBulletChannels = bScatterBulletChannels ? 1u : 0u;
	FRDGTextureRef SparseFieldAtlasTexture = GraphBuilder.RegisterExternalTexture(State.SparseFieldAtlasTexture);
	if (!SparseFieldAtlasTexture)
	{
		return;
	}
	PassParameters->OutSparseFieldAtlas = GraphBuilder.CreateUAV(SparseFieldAtlasTexture);
	PassParameters->SparseDispatchIndirectArgsBuffer = IndirectArgsBuffer;

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.ScatterSparseAtlas SmokeId=%d", State.Volume.SmokeId),
		ComputeShader,
		PassParameters,
		IndirectArgsBuffer,
		0);
}

void FTimeThiefSmokeViewExtension::AddBuildMacDivergencePass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGTextureRef VelocityIn,
	FRDGTextureRef FaceVelocityUOut,
	FRDGTextureRef FaceVelocityVOut,
	FRDGTextureRef FaceVelocityWOut,
	FRDGTextureRef DivergenceOut,
	FRDGTextureRef PressureOut,
	const FActiveBrickDispatchResources* ActiveBrickResources)
{
	const FIntVector GridSize = State.AllocatedGridSize;
	const FVector3f CellSize = MakeCellSize(State.Volume, GridSize);
	const bool bUseActiveBrickDispatch = ActiveBrickResources &&
		ActiveBrickResources->ActiveBrickCountBuffer &&
		ActiveBrickResources->ActiveBricksBuffer &&
		State.bUseSparseSimulationMaskThisFrame;
	FRDGBufferRef ActiveBrickCountBuffer = ActiveBrickResources ? ActiveBrickResources->ActiveBrickCountBuffer : nullptr;
	FRDGBufferRef ActiveBricksBuffer = ActiveBrickResources ? ActiveBrickResources->ActiveBricksBuffer : nullptr;
	if (!ActiveBrickCountBuffer || !ActiveBricksBuffer)
	{
		CreateEmptyActiveBrickBuffers(GraphBuilder, ActiveBrickCountBuffer, ActiveBricksBuffer);
	}

	TShaderMapRef<FTimeThiefSmokeBuildMacDivergenceCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokeBuildMacDivergenceCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeBuildMacDivergenceCS::FParameters>();
	PassParameters->GridResolution = GridSize;
	PassParameters->CellSize = CellSize;
	PassParameters->VelocityIn = VelocityIn;
	PassParameters->BrickOccupancyTexture = GraphBuilder.RegisterExternalTexture(State.BrickOccupancyTexture);
	PassParameters->BrickGridResolution = State.AllocatedBrickGridSize;
	PassParameters->SmokeBrickSize = FMath::Clamp(TimeThiefSmokeParameterDefaults::SmokeBrickSize, TimeThiefSmokeParameterDefaults::SmokeBrickMinSize, TimeThiefSmokeParameterDefaults::SmokeBrickMaxSize);
	PassParameters->MaxActiveSmokeBricks = static_cast<int32>(GetBrickGridCount(State.AllocatedBrickGridSize));
	PassParameters->ActiveBrickCountBuffer = GraphBuilder.CreateSRV(ActiveBrickCountBuffer);
	PassParameters->ActiveBricks = GraphBuilder.CreateSRV(ActiveBricksBuffer);
	PassParameters->bUseSparseSimulationMask = State.bUseSparseSimulationMaskThisFrame ? 1u : 0u;
	PassParameters->bUseActiveBrickDispatch = bUseActiveBrickDispatch ? 1u : 0u;
	TIME_THIEF_SMOKE_SET_OBSTACLE_FIELD_PARAMETERS(PassParameters, GraphBuilder.RegisterExternalTexture(State.ObstacleSdfTexture), GraphBuilder, State);
	PassParameters->OutFaceVelocityU = GraphBuilder.CreateUAV(FaceVelocityUOut);
	PassParameters->OutFaceVelocityV = GraphBuilder.CreateUAV(FaceVelocityVOut);
	PassParameters->OutFaceVelocityW = GraphBuilder.CreateUAV(FaceVelocityWOut);
	PassParameters->OutDivergence = GraphBuilder.CreateUAV(DivergenceOut);
	PassParameters->OutPressure = GraphBuilder.CreateUAV(PressureOut);

	if (bUseActiveBrickDispatch)
	{
		AddClearUAVPass(GraphBuilder, PassParameters->OutFaceVelocityU, 0.0f);
		AddClearUAVPass(GraphBuilder, PassParameters->OutFaceVelocityV, 0.0f);
		AddClearUAVPass(GraphBuilder, PassParameters->OutFaceVelocityW, 0.0f);
		AddClearUAVPass(GraphBuilder, PassParameters->OutDivergence, 0.0f);
		AddClearUAVPass(GraphBuilder, PassParameters->OutPressure, 0.0f);
		FRDGBufferRef IndirectArgsBuffer = ActiveBrickResources->DispatchArgsBuffer
			? ActiveBrickResources->DispatchArgsBuffer
			: AddBuildSparseBrickDispatchArgsPass(
				GraphBuilder,
				State,
				*ActiveBrickResources,
				ComputeSparseScatterGroupsPerBrick(PassParameters->SmokeBrickSize),
				GetBrickGridCount(State.AllocatedBrickGridSize));
		PassParameters->SparseDispatchIndirectArgsBuffer = IndirectArgsBuffer;
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("TimeThiefSmoke.BuildMacDivergenceActive SmokeId=%d", State.Volume.SmokeId),
			ComputeShader,
			PassParameters,
			IndirectArgsBuffer,
			0);
		return;
	}

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.BuildMacDivergence SmokeId=%d", State.Volume.SmokeId),
		ComputeShader,
		PassParameters,
		MakeGroupCount(GridSize));
}

void FTimeThiefSmokeViewExtension::AddPressureJacobiPass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	const FIntVector& GridSize,
	const FVector3f& CellSize,
	FRDGTextureRef PressureIn,
	FRDGTextureRef DivergenceIn,
	FRDGTextureRef ObstacleTexture,
	FRDGTextureRef PressureOut,
	const FActiveBrickDispatchResources* ActiveBrickResources)
{
	const bool bUseActiveBrickDispatch = ActiveBrickResources &&
		ActiveBrickResources->ActiveBrickCountBuffer &&
		ActiveBrickResources->ActiveBricksBuffer &&
		State.bUseSparseSimulationMaskThisFrame &&
		GridSize == State.AllocatedGridSize;
	FRDGBufferRef ActiveBrickCountBuffer = ActiveBrickResources ? ActiveBrickResources->ActiveBrickCountBuffer : nullptr;
	FRDGBufferRef ActiveBricksBuffer = ActiveBrickResources ? ActiveBrickResources->ActiveBricksBuffer : nullptr;
	if (!ActiveBrickCountBuffer || !ActiveBricksBuffer)
	{
		CreateEmptyActiveBrickBuffers(GraphBuilder, ActiveBrickCountBuffer, ActiveBricksBuffer);
	}

	TShaderMapRef<FTimeThiefSmokePressureJacobiCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokePressureJacobiCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokePressureJacobiCS::FParameters>();
	PassParameters->GridResolution = GridSize;
	PassParameters->CellSize = CellSize;
	PassParameters->PressureIn = PressureIn;
	PassParameters->DivergenceIn = DivergenceIn;
	PassParameters->BrickOccupancyTexture = GraphBuilder.RegisterExternalTexture(State.BrickOccupancyTexture);
	PassParameters->BrickGridResolution = State.AllocatedBrickGridSize;
	PassParameters->SmokeBrickSize = FMath::Clamp(TimeThiefSmokeParameterDefaults::SmokeBrickSize, TimeThiefSmokeParameterDefaults::SmokeBrickMinSize, TimeThiefSmokeParameterDefaults::SmokeBrickMaxSize);
	PassParameters->MaxActiveSmokeBricks = static_cast<int32>(GetBrickGridCount(State.AllocatedBrickGridSize));
	PassParameters->ActiveBrickCountBuffer = GraphBuilder.CreateSRV(ActiveBrickCountBuffer);
	PassParameters->ActiveBricks = GraphBuilder.CreateSRV(ActiveBricksBuffer);
	PassParameters->bUseSparseSimulationMask = State.bUseSparseSimulationMaskThisFrame && GridSize == State.AllocatedGridSize ? 1u : 0u;
	PassParameters->bUseActiveBrickDispatch = bUseActiveBrickDispatch ? 1u : 0u;
	TIME_THIEF_SMOKE_SET_OBSTACLE_FIELD_PARAMETERS(PassParameters, ObstacleTexture, GraphBuilder, State);
	PassParameters->OutPressure = GraphBuilder.CreateUAV(PressureOut);

	if (bUseActiveBrickDispatch)
	{
		AddClearUAVPass(GraphBuilder, PassParameters->OutPressure, 0.0f);
		FRDGBufferRef IndirectArgsBuffer = ActiveBrickResources->DispatchArgsBuffer
			? ActiveBrickResources->DispatchArgsBuffer
			: AddBuildSparseBrickDispatchArgsPass(
				GraphBuilder,
				State,
				*ActiveBrickResources,
				ComputeSparseScatterGroupsPerBrick(PassParameters->SmokeBrickSize),
				GetBrickGridCount(State.AllocatedBrickGridSize));
		PassParameters->SparseDispatchIndirectArgsBuffer = IndirectArgsBuffer;
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("TimeThiefSmoke.PressureJacobiActive SmokeId=%d", State.Volume.SmokeId),
			ComputeShader,
			PassParameters,
			IndirectArgsBuffer,
			0);
		return;
	}

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.PressureJacobi SmokeId=%d", State.Volume.SmokeId),
		ComputeShader,
		PassParameters,
		MakeGroupCount(GridSize));
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
	TIME_THIEF_SMOKE_SET_OBSTACLE_FIELD_PARAMETERS(PassParameters, GraphBuilder.RegisterExternalTexture(State.ObstacleSdfTexture), GraphBuilder, State);
	PassParameters->OutVelocity = GraphBuilder.CreateUAV(VelocityOut);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.ProjectVelocity SmokeId=%d", State.Volume.SmokeId),
		ComputeShader,
		PassParameters,
		GroupCount);
}

void FTimeThiefSmokeViewExtension::AddProjectMacToCollocatedVelocityPass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGTextureRef FaceVelocityUIn,
	FRDGTextureRef FaceVelocityVIn,
	FRDGTextureRef FaceVelocityWIn,
	FRDGTextureRef PressureIn,
	FRDGTextureRef VelocityOut,
	const FActiveBrickDispatchResources* ActiveBrickResources)
{
	const FIntVector GridSize = State.AllocatedGridSize;
	const FVector3f CellSize = MakeCellSize(State.Volume, GridSize);
	const bool bUseActiveBrickDispatch = ActiveBrickResources &&
		ActiveBrickResources->ActiveBrickCountBuffer &&
		ActiveBrickResources->ActiveBricksBuffer &&
		State.bUseSparseSimulationMaskThisFrame;
	FRDGBufferRef ActiveBrickCountBuffer = ActiveBrickResources ? ActiveBrickResources->ActiveBrickCountBuffer : nullptr;
	FRDGBufferRef ActiveBricksBuffer = ActiveBrickResources ? ActiveBrickResources->ActiveBricksBuffer : nullptr;
	if (!ActiveBrickCountBuffer || !ActiveBricksBuffer)
	{
		CreateEmptyActiveBrickBuffers(GraphBuilder, ActiveBrickCountBuffer, ActiveBricksBuffer);
	}

	TShaderMapRef<FTimeThiefSmokeProjectMacToCollocatedVelocityCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokeProjectMacToCollocatedVelocityCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeProjectMacToCollocatedVelocityCS::FParameters>();
	PassParameters->GridResolution = GridSize;
	PassParameters->CellSize = CellSize;
	PassParameters->FaceVelocityUIn = FaceVelocityUIn;
	PassParameters->FaceVelocityVIn = FaceVelocityVIn;
	PassParameters->FaceVelocityWIn = FaceVelocityWIn;
	PassParameters->PressureIn = PressureIn;
	PassParameters->BrickOccupancyTexture = GraphBuilder.RegisterExternalTexture(State.BrickOccupancyTexture);
	PassParameters->BrickGridResolution = State.AllocatedBrickGridSize;
	PassParameters->SmokeBrickSize = FMath::Clamp(TimeThiefSmokeParameterDefaults::SmokeBrickSize, TimeThiefSmokeParameterDefaults::SmokeBrickMinSize, TimeThiefSmokeParameterDefaults::SmokeBrickMaxSize);
	PassParameters->MaxActiveSmokeBricks = static_cast<int32>(GetBrickGridCount(State.AllocatedBrickGridSize));
	PassParameters->ActiveBrickCountBuffer = GraphBuilder.CreateSRV(ActiveBrickCountBuffer);
	PassParameters->ActiveBricks = GraphBuilder.CreateSRV(ActiveBricksBuffer);
	PassParameters->bUseSparseSimulationMask = State.bUseSparseSimulationMaskThisFrame ? 1u : 0u;
	PassParameters->bUseActiveBrickDispatch = bUseActiveBrickDispatch ? 1u : 0u;
	TIME_THIEF_SMOKE_SET_OBSTACLE_FIELD_PARAMETERS(PassParameters, GraphBuilder.RegisterExternalTexture(State.ObstacleSdfTexture), GraphBuilder, State);
	PassParameters->OutVelocity = GraphBuilder.CreateUAV(VelocityOut);

	if (bUseActiveBrickDispatch)
	{
		AddClearUAVPass(GraphBuilder, PassParameters->OutVelocity, 0.0f);
		FRDGBufferRef IndirectArgsBuffer = ActiveBrickResources->DispatchArgsBuffer
			? ActiveBrickResources->DispatchArgsBuffer
			: AddBuildSparseBrickDispatchArgsPass(
				GraphBuilder,
				State,
				*ActiveBrickResources,
				ComputeSparseScatterGroupsPerBrick(PassParameters->SmokeBrickSize),
				GetBrickGridCount(State.AllocatedBrickGridSize));
		PassParameters->SparseDispatchIndirectArgsBuffer = IndirectArgsBuffer;
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("TimeThiefSmoke.ProjectMacToCollocatedVelocityActive SmokeId=%d", State.Volume.SmokeId),
			ComputeShader,
			PassParameters,
			IndirectArgsBuffer,
			0);
		return;
	}

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.ProjectMacToCollocatedVelocity SmokeId=%d", State.Volume.SmokeId),
		ComputeShader,
		PassParameters,
		MakeGroupCount(GridSize));
}
