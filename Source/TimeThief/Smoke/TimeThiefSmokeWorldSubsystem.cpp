#include "Smoke/TimeThiefSmokeWorldSubsystem.h"

#include "Actors/TimeThiefSmokeVolume.h"
#include "Engine/Engine.h"
#include "Stats/Stats.h"
#include "TimeThiefSmokeParameterDefaults.h"
#include "TimeThiefSmokeRendererSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

namespace TimeThiefSmoke
{
	struct FPendingRendererSmokeVolume
	{
		FTimeThiefSmokeRendererVolume Volume;
		FBox ClusterBounds = FBox(EForceInit::ForceInit);
	};

	const FTimeThiefSmokeRendererSettings& GetDefaultRendererSettings()
	{
		static const FTimeThiefSmokeRendererSettings Settings;
		return Settings;
	}

	FBox MakeSmokeWorldBounds(const FTimeThiefSmokeRendererVolume& Volume, const FVector3f& Extent)
	{
		const FVector3f SafeExtent = FVector3f(
			FMath::Max(Extent.X, 1.0f),
			FMath::Max(Extent.Y, 1.0f),
			FMath::Max(Extent.Z, 1.0f));
		FBox Bounds(EForceInit::ForceInit);

		for (int32 Z = -1; Z <= 1; Z += 2)
		{
			for (int32 Y = -1; Y <= 1; Y += 2)
			{
				for (int32 X = -1; X <= 1; X += 2)
				{
					const FVector3f LocalCorner(SafeExtent.X * static_cast<float>(X), SafeExtent.Y * static_cast<float>(Y), SafeExtent.Z * static_cast<float>(Z));
					const FVector3f WorldCorner = Volume.LocalToWorld.TransformPosition(LocalCorner);
					Bounds += FVector(WorldCorner.X, WorldCorner.Y, WorldCorner.Z);
				}
			}
		}

		return Bounds;
	}

	FBox MakeSmokeClusterWorldBounds(const FTimeThiefSmokeRendererVolume& Volume)
	{
		return MakeSmokeWorldBounds(Volume, Volume.NaturalBoundsExtent);
	}

	FBox ExpandSmokeClusterBounds(const FBox& Bounds, const float ExpansionRatio, const float MinExpansionCm)
	{
		const FVector Extent = Bounds.GetExtent();
		const FVector Padding(
			FMath::Max(Extent.X * ExpansionRatio, MinExpansionCm),
			FMath::Max(Extent.Y * ExpansionRatio, MinExpansionCm),
			FMath::Max(Extent.Z * ExpansionRatio, MinExpansionCm));
		return Bounds.ExpandBy(Padding);
	}

	uint64 MakeSmokePairKey(const int32 SmokeIdA, const int32 SmokeIdB)
	{
		const uint32 Low = static_cast<uint32>(FMath::Min(SmokeIdA, SmokeIdB));
		const uint32 High = static_cast<uint32>(FMath::Max(SmokeIdA, SmokeIdB));
		return (static_cast<uint64>(Low) << 32) | static_cast<uint64>(High);
	}

	bool HasSolidObstacleField(const FTimeThiefSmokeRendererVolume& Volume)
	{
		return Volume.bHasSolidObstacleField;
	}

	FIntVector WorldToSmokeSpatialCell(const FVector& Position)
	{
		const float SafeCellSize = FMath::Max(TimeThiefSmokeParameterDefaults::SmokeSpatialCellSize, 1.0f);
		return FIntVector(
			FMath::FloorToInt(Position.X / SafeCellSize),
			FMath::FloorToInt(Position.Y / SafeCellSize),
			FMath::FloorToInt(Position.Z / SafeCellSize));
	}

	FBox MakeTraceQueryBounds(const FVector& TraceStart, const FVector& TraceEnd)
	{
		FBox Bounds(EForceInit::ForceInit);
		Bounds += TraceStart;
		Bounds += TraceEnd;
		return Bounds.ExpandBy(1.0f);
	}

	bool AreSmokeVolumesClusterCompatible(const FTimeThiefSmokeRendererVolume& A, const FTimeThiefSmokeRendererVolume& B)
	{
		const bool bObstacleCompatible =
			(A.Settings.bEnableClusterObstacleMerge && B.Settings.bEnableClusterObstacleMerge) ||
			(!HasSolidObstacleField(A) && !HasSolidObstacleField(B));
		return A.Settings.SimulationBackend == ETimeThiefSmokeSimulationBackend::SparseMac &&
			B.Settings.SimulationBackend == ETimeThiefSmokeSimulationBackend::SparseMac &&
			A.Settings.SmokeGridResolution == B.Settings.SmokeGridResolution &&
			A.Settings.SmokeBrickSize == B.Settings.SmokeBrickSize &&
			A.Settings.MaxActiveSmokeBricks == B.Settings.MaxActiveSmokeBricks &&
			bObstacleCompatible;
	}

	int32 FindClusterRoot(TArray<int32>& Parents, const int32 Index)
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

	void UnionSmokeClusters(TArray<int32>& Parents, const int32 A, const int32 B)
	{
		const int32 RootA = FindClusterRoot(Parents, A);
		const int32 RootB = FindClusterRoot(Parents, B);
		if (RootA != INDEX_NONE && RootB != INDEX_NONE && RootA != RootB)
		{
			Parents[RootB] = RootA;
		}
	}

	uint64 MakeIndexPairKey(const int32 A, const int32 B)
	{
		const uint32 Low = static_cast<uint32>(FMath::Min(A, B));
		const uint32 High = static_cast<uint32>(FMath::Max(A, B));
		return (static_cast<uint64>(Low) << 32) | static_cast<uint64>(High);
	}

	void AddClusterCandidatePairsForBounds(
		const int32 Index,
		const FBox& Bounds,
		TMap<FIntVector, TArray<int32>>& Cells,
		TSet<uint64>& PairKeys,
		TArray<TPair<int32, int32>>& OutPairs)
	{
		if (!Bounds.IsValid)
		{
			return;
		}

		const FIntVector MinCell = WorldToSmokeSpatialCell(Bounds.Min);
		const FIntVector MaxCell = WorldToSmokeSpatialCell(Bounds.Max);
		for (int32 Z = MinCell.Z; Z <= MaxCell.Z; ++Z)
		{
			for (int32 Y = MinCell.Y; Y <= MaxCell.Y; ++Y)
			{
				for (int32 X = MinCell.X; X <= MaxCell.X; ++X)
				{
					TArray<int32>& CellIndices = Cells.FindOrAdd(FIntVector(X, Y, Z));
					for (const int32 OtherIndex : CellIndices)
					{
						if (OtherIndex == Index)
						{
							continue;
						}

						const uint64 PairKey = MakeIndexPairKey(Index, OtherIndex);
						if (!PairKeys.Contains(PairKey))
						{
							PairKeys.Add(PairKey);
							OutPairs.Add(TPair<int32, int32>(FMath::Min(Index, OtherIndex), FMath::Max(Index, OtherIndex)));
						}
					}
					CellIndices.AddUnique(Index);
				}
			}
		}
	}

	void BuildSmokeClusterCandidatePairs(
		const TArray<FBox>& ExpandedClusterBounds,
		const TArray<FBox>& ReleaseClusterBounds,
		TArray<TPair<int32, int32>>& OutPairs)
	{
		OutPairs.Reset();
		TMap<FIntVector, TArray<int32>> Cells;
		TSet<uint64> PairKeys;
		for (int32 Index = 0; Index < ExpandedClusterBounds.Num(); ++Index)
		{
			AddClusterCandidatePairsForBounds(Index, ExpandedClusterBounds[Index], Cells, PairKeys, OutPairs);
			AddClusterCandidatePairsForBounds(Index, ReleaseClusterBounds[Index], Cells, PairKeys, OutPairs);
		}
	}

	void AssignSmokeClusters(TArray<FPendingRendererSmokeVolume>& PendingVolumes, TSet<uint64>& PersistentClusterLinks)
	{
		TArray<int32> Parents;
		Parents.SetNumUninitialized(PendingVolumes.Num());
		for (int32 Index = 0; Index < Parents.Num(); ++Index)
		{
			Parents[Index] = Index;
			PendingVolumes[Index].Volume.ClusterId = PendingVolumes[Index].Volume.SmokeId;
			PendingVolumes[Index].Volume.ClusterSourceCount = 1;
		}

		if (PendingVolumes.Num() <= 1)
		{
			PersistentClusterLinks.Reset();
			return;
		}

		TArray<FBox> ExpandedClusterBounds;
		ExpandedClusterBounds.Reserve(PendingVolumes.Num());
		TArray<FBox> ReleaseClusterBounds;
		ReleaseClusterBounds.Reserve(PendingVolumes.Num());
		for (const FPendingRendererSmokeVolume& PendingVolume : PendingVolumes)
		{
			ExpandedClusterBounds.Add(ExpandSmokeClusterBounds(PendingVolume.ClusterBounds, TimeThiefSmokeParameterDefaults::SmokeClusterBoundsExpansionRatio, TimeThiefSmokeParameterDefaults::SmokeClusterMinExpansionCm));
			ReleaseClusterBounds.Add(ExpandSmokeClusterBounds(PendingVolume.ClusterBounds, TimeThiefSmokeParameterDefaults::SmokeClusterReleaseBoundsExpansionRatio, TimeThiefSmokeParameterDefaults::SmokeClusterReleaseMinExpansionCm));
		}

		TSet<uint64> NextPersistentClusterLinks;
		TArray<TPair<int32, int32>> CandidatePairs;
		BuildSmokeClusterCandidatePairs(ExpandedClusterBounds, ReleaseClusterBounds, CandidatePairs);
		for (const TPair<int32, int32>& CandidatePair : CandidatePairs)
		{
			const int32 A = CandidatePair.Key;
			const int32 B = CandidatePair.Value;
			if (!PendingVolumes.IsValidIndex(A) || !PendingVolumes.IsValidIndex(B))
			{
				continue;
			}

			if (!AreSmokeVolumesClusterCompatible(PendingVolumes[A].Volume, PendingVolumes[B].Volume))
			{
				continue;
			}

			const uint64 PairKey = MakeSmokePairKey(PendingVolumes[A].Volume.SmokeId, PendingVolumes[B].Volume.SmokeId);
			const bool bJoinNow = ExpandedClusterBounds[A].Intersect(ExpandedClusterBounds[B]);
			const bool bKeepExisting = PersistentClusterLinks.Contains(PairKey) &&
				ReleaseClusterBounds[A].Intersect(ReleaseClusterBounds[B]);
			if (bJoinNow || bKeepExisting)
			{
				UnionSmokeClusters(Parents, A, B);
				NextPersistentClusterLinks.Add(PairKey);
			}
		}
		PersistentClusterLinks = MoveTemp(NextPersistentClusterLinks);

		TMap<int32, int32> ClusterIdsByRoot;
		TMap<int32, int32> ClusterCountsByRoot;
		for (int32 Index = 0; Index < PendingVolumes.Num(); ++Index)
		{
			const int32 Root = FindClusterRoot(Parents, Index);
			if (Root == INDEX_NONE)
			{
				continue;
			}

			int32& ClusterId = ClusterIdsByRoot.FindOrAdd(Root, PendingVolumes[Index].Volume.SmokeId);
			ClusterId = FMath::Min(ClusterId, PendingVolumes[Index].Volume.SmokeId);
			int32& ClusterSourceCount = ClusterCountsByRoot.FindOrAdd(Root);
			++ClusterSourceCount;
		}

		for (int32 Index = 0; Index < PendingVolumes.Num(); ++Index)
		{
			const int32 Root = FindClusterRoot(Parents, Index);
			if (const int32* ClusterId = ClusterIdsByRoot.Find(Root))
			{
				PendingVolumes[Index].Volume.ClusterId = *ClusterId;
			}
			if (const int32* ClusterSourceCount = ClusterCountsByRoot.Find(Root))
			{
				PendingVolumes[Index].Volume.ClusterSourceCount = FMath::Max(*ClusterSourceCount, 1);
			}
		}
	}

	FVector3f MakeExtentAroundCenter(const FBox& Bounds, const FVector& Center)
	{
		if (!Bounds.IsValid)
		{
			return FVector3f::ZeroVector;
		}

		const FVector MinDelta = (Bounds.Min - Center).GetAbs();
		const FVector MaxDelta = (Bounds.Max - Center).GetAbs();
		return FVector3f(
			static_cast<float>(FMath::Max(MinDelta.X, MaxDelta.X)),
			static_cast<float>(FMath::Max(MinDelta.Y, MaxDelta.Y)),
			static_cast<float>(FMath::Max(MinDelta.Z, MaxDelta.Z)));
	}

	float GetObstaclePrimitivePriorityRadius(const FTimeThiefSmokeObstaclePrimitive& Primitive)
	{
		return FMath::Max3(
			FMath::Max(Primitive.CenterRadius.W, 1.0f),
			FMath::Max(Primitive.ExtentsShape.X, 1.0f),
			FMath::Max(Primitive.ExtentsShape.Y, Primitive.ExtentsShape.Z));
	}

	FTimeThiefSmokeObstaclePrimitive TransformObstaclePrimitiveToClusterLocal(
		const FTimeThiefSmokeObstaclePrimitive& Primitive,
		const FTransform3f& MemberLocalToWorld,
		const FTransform3f& ClusterLocalToWorld)
	{
		FTimeThiefSmokeObstaclePrimitive Result = Primitive;
		const FVector3f MemberCenter(Primitive.CenterRadius.X, Primitive.CenterRadius.Y, Primitive.CenterRadius.Z);
		const FVector3f WorldCenter = MemberLocalToWorld.TransformPosition(MemberCenter);
		const FVector3f ClusterCenter = ClusterLocalToWorld.InverseTransformPosition(WorldCenter);
		Result.CenterRadius.X = ClusterCenter.X;
		Result.CenterRadius.Y = ClusterCenter.Y;
		Result.CenterRadius.Z = ClusterCenter.Z;

		const FQuat4f ClusterFromMember = ClusterLocalToWorld.GetRotation().Inverse() * MemberLocalToWorld.GetRotation();
		const ETimeThiefSmokeObstaclePrimitiveShape Shape = static_cast<ETimeThiefSmokeObstaclePrimitiveShape>(FMath::RoundToInt(Primitive.ExtentsShape.W));
		if (Shape == ETimeThiefSmokeObstaclePrimitiveShape::Capsule)
		{
			const FVector3f MemberAxis(Primitive.AxisHalfLength.X, Primitive.AxisHalfLength.Y, Primitive.AxisHalfLength.Z);
			const FVector3f ClusterAxis = ClusterFromMember.RotateVector(MemberAxis).GetSafeNormal(UE_SMALL_NUMBER, FVector3f::UpVector);
			Result.AxisHalfLength.X = ClusterAxis.X;
			Result.AxisHalfLength.Y = ClusterAxis.Y;
			Result.AxisHalfLength.Z = ClusterAxis.Z;
		}
		else if (Shape == ETimeThiefSmokeObstaclePrimitiveShape::Box || Shape == ETimeThiefSmokeObstaclePrimitiveShape::Aabb)
		{
			const FQuat4f PrimitiveRotation(Primitive.Rotation.X, Primitive.Rotation.Y, Primitive.Rotation.Z, Primitive.Rotation.W);
			const FQuat4f ClusterRotation = Shape == ETimeThiefSmokeObstaclePrimitiveShape::Aabb
				? ClusterFromMember.GetNormalized()
				: (ClusterFromMember * PrimitiveRotation).GetNormalized();
			Result.Rotation = FVector4f(ClusterRotation.X, ClusterRotation.Y, ClusterRotation.Z, ClusterRotation.W);
			Result.ExtentsShape.W = static_cast<float>(static_cast<uint8>(ETimeThiefSmokeObstaclePrimitiveShape::Box));
		}

		return Result;
	}

	void MergeClusterObstaclePrimitives(
		const TArray<FPendingRendererSmokeVolume*>& ClusterMembers,
		FTimeThiefSmokeRendererVolume& ClusterVolume)
	{
		struct FMergedObstaclePrimitive
		{
			FTimeThiefSmokeObstaclePrimitive Primitive;
			float Priority = 0.0f;
		};

		TArray<FMergedObstaclePrimitive> Candidates;
		uint32 RevisionHash = 0u;
		int32 MaxResolution = 0;
		for (const FPendingRendererSmokeVolume* Member : ClusterMembers)
		{
			if (!Member || !Member->Volume.bHasSolidObstacleField)
			{
				continue;
			}

			const FTimeThiefSmokeRendererVolume& Volume = Member->Volume;
			MaxResolution = FMath::Max(MaxResolution, Volume.ObstacleFieldResolution);
			RevisionHash = HashCombineFast(RevisionHash, GetTypeHash(Volume.SmokeId));
			RevisionHash = HashCombineFast(RevisionHash, Volume.ObstacleFieldRevision);
			RevisionHash = HashCombineFast(RevisionHash, GetTypeHash(Volume.ObstaclePrimitives.Num()));
			for (const FTimeThiefSmokeObstaclePrimitive& Primitive : Volume.ObstaclePrimitives)
			{
				FMergedObstaclePrimitive& Candidate = Candidates.AddDefaulted_GetRef();
				Candidate.Primitive = TransformObstaclePrimitiveToClusterLocal(Primitive, Volume.LocalToWorld, ClusterVolume.LocalToWorld);
				const FVector3f Center(Candidate.Primitive.CenterRadius.X, Candidate.Primitive.CenterRadius.Y, Candidate.Primitive.CenterRadius.Z);
				const float Radius = GetObstaclePrimitivePriorityRadius(Candidate.Primitive);
				Candidate.Priority = Center.SizeSquared() / FMath::Max(Radius * Radius, 1.0f);
			}
		}

		if (Candidates.IsEmpty())
		{
			ClusterVolume.ObstacleFieldResolution = 0;
			ClusterVolume.ObstacleFieldRevision = 0;
			ClusterVolume.ObstaclePrimitives.Reset();
			ClusterVolume.bHasSolidObstacleField = false;
			return;
		}

		Candidates.Sort([](const FMergedObstaclePrimitive& Left, const FMergedObstaclePrimitive& Right)
		{
			return Left.Priority < Right.Priority;
		});

		const int32 PrimitiveLimit = TimeThiefSmokeParameterDefaults::MaxObstaclePrimitives;
		ClusterVolume.ObstaclePrimitives.Reset(FMath::Min(Candidates.Num(), PrimitiveLimit));
		for (int32 CandidateIndex = 0; CandidateIndex < Candidates.Num() && CandidateIndex < PrimitiveLimit; ++CandidateIndex)
		{
			ClusterVolume.ObstaclePrimitives.Add(Candidates[CandidateIndex].Primitive);
		}
		ClusterVolume.ObstacleFieldResolution = MaxResolution;
		ClusterVolume.ObstacleFieldRevision = RevisionHash != 0u ? RevisionHash : 1u;
		ClusterVolume.bHasSolidObstacleField = !ClusterVolume.ObstaclePrimitives.IsEmpty();
	}

	bool TryMakeClusterSourceEvent(const FTimeThiefSmokeRendererVolume& Volume, const int32 ClusterSmokeId, FTimeThiefSmokeRendererEvent& OutSourceEvent)
	{
		const float NormalizedAge = Volume.Settings.PlumeEmissionDuration > KINDA_SMALL_NUMBER
			? Volume.AgeSeconds / Volume.Settings.PlumeEmissionDuration
			: 1.0f;
		const float SourceAge = FMath::Max(NormalizedAge, 0.0f) * FMath::Max(Volume.Settings.PlumeEmissionDuration, 0.01f);
		const float EmissionRamp = FMath::Clamp((SourceAge + KINDA_SMALL_NUMBER) / 0.35f, 0.0f, 1.0f);
		const float EmissionFade = 1.0f - FMath::SmoothStep(
			FMath::Max(Volume.Settings.PlumeEmissionDuration, 0.01f),
			FMath::Max(Volume.Settings.PlumeEmissionDuration, 0.01f) + 0.25f,
			SourceAge);
		if (EmissionRamp * EmissionFade <= TimeThiefSmokeParameterDefaults::SimulationEventMinStrength)
		{
			return false;
		}

		OutSourceEvent.SmokeId = ClusterSmokeId;
		OutSourceEvent.Type = ETimeThiefSmokeRendererInteractionType::PlumeSource;
		OutSourceEvent.Shape = ETimeThiefSmokeRendererInteractionShape::Sphere;
		OutSourceEvent.Position = Volume.LocalToWorld.GetLocation();
		OutSourceEvent.PreviousPosition = OutSourceEvent.Position;
		OutSourceEvent.Direction = Volume.LocalToWorld.TransformVector(FVector3f(0.0f, 0.0f, 1.0f)).GetSafeNormal();
		OutSourceEvent.Radius = FMath::Max(Volume.Settings.PlumeSourceRadius, 1.0f);
		OutSourceEvent.Length = OutSourceEvent.Radius * 2.0f;
		OutSourceEvent.NormalizedAge = NormalizedAge;
		OutSourceEvent.Strength = 1.0f;
		OutSourceEvent.Seed = Volume.SmokeId;
		return true;
	}

	FTimeThiefSmokeRendererVolume MakeClusterVolume(const TArray<FPendingRendererSmokeVolume*>& ClusterMembers)
	{
		check(!ClusterMembers.IsEmpty());

		const FTimeThiefSmokeRendererVolume& FirstVolume = ClusterMembers[0]->Volume;
		FTimeThiefSmokeRendererVolume ClusterVolume = FirstVolume;
		ClusterVolume.SmokeId = FirstVolume.ClusterId;
		ClusterVolume.ClusterId = FirstVolume.ClusterId;
		ClusterVolume.ClusterSourceCount = ClusterMembers.Num();
		ClusterVolume.AgeSeconds = FirstVolume.AgeSeconds;
		ClusterVolume.DurationSeconds = FirstVolume.DurationSeconds;
		ClusterVolume.ObstacleFieldResolution = 0;
		ClusterVolume.ObstacleFieldRevision = 0;
		ClusterVolume.ObstaclePrimitives.Reset();
		ClusterVolume.bHasSolidObstacleField = false;
		ClusterVolume.SourceEvents.Reset();
		ClusterVolume.SourceEvents.Reserve(ClusterMembers.Num());

		FBox NaturalBounds(EForceInit::ForceInit);
		FBox RenderBounds(EForceInit::ForceInit);
		FBox SimulationBounds(EForceInit::ForceInit);
		for (const FPendingRendererSmokeVolume* Member : ClusterMembers)
		{
			const FTimeThiefSmokeRendererVolume& Volume = Member->Volume;
			NaturalBounds += MakeSmokeWorldBounds(Volume, Volume.NaturalBoundsExtent);
			RenderBounds += MakeSmokeWorldBounds(Volume, Volume.RenderBoundsExtent);
			SimulationBounds += MakeSmokeWorldBounds(Volume, Volume.SimulationBoundsExtent);
			ClusterVolume.AgeSeconds = FMath::Min(ClusterVolume.AgeSeconds, Volume.AgeSeconds);
			ClusterVolume.DurationSeconds = FMath::Max(ClusterVolume.DurationSeconds, Volume.DurationSeconds);
		}

		FBox ClusterBounds(EForceInit::ForceInit);
		ClusterBounds += NaturalBounds;
		ClusterBounds += RenderBounds;
		ClusterBounds += SimulationBounds;
		const FVector ClusterCenter = ClusterBounds.GetCenter();
		const FVector3f ClusterCenterFloat(
			static_cast<float>(ClusterCenter.X),
			static_cast<float>(ClusterCenter.Y),
			static_cast<float>(ClusterCenter.Z));
		ClusterVolume.LocalToWorld = FTransform3f(FQuat4f::Identity, ClusterCenterFloat, FVector3f(1.0f, 1.0f, 1.0f));
		ClusterVolume.NaturalBoundsExtent = MakeExtentAroundCenter(NaturalBounds, ClusterCenter);
		ClusterVolume.RenderBoundsExtent = MakeExtentAroundCenter(RenderBounds, ClusterCenter);
		ClusterVolume.SimulationBoundsExtent = MakeExtentAroundCenter(SimulationBounds, ClusterCenter);
		ClusterVolume.BoundsExtent = ClusterVolume.SimulationBoundsExtent;
		if (ClusterVolume.Settings.bEnableClusterObstacleMerge)
		{
			MergeClusterObstaclePrimitives(ClusterMembers, ClusterVolume);
		}

		for (const FPendingRendererSmokeVolume* Member : ClusterMembers)
		{
			FTimeThiefSmokeRendererEvent SourceEvent;
			if (TryMakeClusterSourceEvent(Member->Volume, ClusterVolume.SmokeId, SourceEvent))
			{
				ClusterVolume.SourceEvents.Add(SourceEvent);
			}
		}

		return ClusterVolume;
	}

	float ComputeInteractionEventPriority(const FTimeThiefSmokeInteractionEvent& Event, const float Age, const float Duration)
	{
		const float NormalizedAge = Duration > KINDA_SMALL_NUMBER ? FMath::Clamp(Age / Duration, 0.0f, 1.0f) : 1.0f;
		const float AgeWeight = FMath::Max(1.0f - NormalizedAge, TimeThiefSmokeParameterDefaults::EventPriorityMinAgeWeight);
		const float Strength = FMath::Max(Event.Strength, TimeThiefSmokeParameterDefaults::EventPriorityMinStrength);
		const float RadiusWeight = FMath::Clamp(
			Event.Radius / TimeThiefSmokeParameterDefaults::EventPriorityRadiusDivisor,
			TimeThiefSmokeParameterDefaults::EventPriorityRadiusMin,
			TimeThiefSmokeParameterDefaults::EventPriorityRadiusMax);
		return Strength * RadiusWeight * AgeWeight;
	}

	bool AddPrioritizedActiveImpulse(TArray<FTimeThiefActiveSmokeImpulse>& ActiveImpulses, const FTimeThiefActiveSmokeImpulse& NewImpulse)
	{
		int32 MatchingCount = 0;
		int32 LowestPriorityIndex = INDEX_NONE;
		float LowestPriority = TNumericLimits<float>::Max();

		for (int32 Index = 0; Index < ActiveImpulses.Num(); ++Index)
		{
			FTimeThiefActiveSmokeImpulse& ActiveImpulse = ActiveImpulses[Index];
			if (ActiveImpulse.SmokeVolume.Get() != NewImpulse.SmokeVolume.Get() || ActiveImpulse.Event.Type != NewImpulse.Event.Type)
			{
				continue;
			}

			++MatchingCount;
			const float Priority = ComputeInteractionEventPriority(ActiveImpulse.Event, ActiveImpulse.Age, ActiveImpulse.Duration);
			if (Priority < LowestPriority)
			{
				LowestPriority = Priority;
				LowestPriorityIndex = Index;
			}
		}

		if (MatchingCount < TimeThiefSmokeParameterDefaults::MaxActiveExplosionImpulsesPerSmoke)
		{
			ActiveImpulses.Add(NewImpulse);
			return true;
		}

		const float NewPriority = ComputeInteractionEventPriority(NewImpulse.Event, NewImpulse.Age, NewImpulse.Duration);
		if (LowestPriorityIndex != INDEX_NONE && NewPriority > LowestPriority)
		{
			ActiveImpulses[LowestPriorityIndex] = NewImpulse;
			return true;
		}

		return false;
	}

	ETimeThiefSmokeRendererInteractionType ToRendererType(ESmokeInteractionType Type)
	{
		switch (Type)
		{
		case ESmokeInteractionType::ExplosionShock:
			return ETimeThiefSmokeRendererInteractionType::ExplosionShock;
		case ESmokeInteractionType::ActorPush:
			return ETimeThiefSmokeRendererInteractionType::ActorPush;
		case ESmokeInteractionType::BulletWake:
		default:
			return ETimeThiefSmokeRendererInteractionType::BulletWake;
		}
	}

	ETimeThiefSmokeRendererInteractionShape ToRendererShape(ESmokeInteractionShape Shape)
	{
		switch (Shape)
		{
		case ESmokeInteractionShape::Capsule:
			return ETimeThiefSmokeRendererInteractionShape::Capsule;
		case ESmokeInteractionShape::Box:
			return ETimeThiefSmokeRendererInteractionShape::Box;
		case ESmokeInteractionShape::LineWake:
			return ETimeThiefSmokeRendererInteractionShape::LineWake;
		case ESmokeInteractionShape::Sphere:
		default:
			return ETimeThiefSmokeRendererInteractionShape::Sphere;
		}
	}

	FTimeThiefSmokeRendererEvent ToRendererEvent(const FTimeThiefSmokeInteractionEvent& Event)
	{
		FTimeThiefSmokeRendererEvent RendererEvent;
		RendererEvent.SmokeId = Event.SmokeId;
		RendererEvent.Type = ToRendererType(Event.Type);
		RendererEvent.Shape = ToRendererShape(Event.Shape);
		RendererEvent.Position = FVector3f(Event.Position);
		RendererEvent.PreviousPosition = FVector3f(Event.PreviousPosition);
		RendererEvent.Direction = FVector3f(Event.Direction);
		RendererEvent.Rotation = FQuat4f(Event.Rotation);
		RendererEvent.Extents = FVector3f(Event.Extents);
		RendererEvent.Radius = Event.Radius;
		RendererEvent.Length = Event.Length;
		RendererEvent.Strength = Event.Strength;
		RendererEvent.Speed = Event.Speed;
		RendererEvent.WarpBudget = Event.WarpBudget;
		RendererEvent.NormalizedAge = Event.NormalizedAge;
		RendererEvent.Seed = Event.Seed;
		return RendererEvent;
	}

#if WITH_DEV_AUTOMATION_TESTS
	FBox MakeAutomationClusterBox(const FVector& Center, const FVector& Extent)
	{
		return FBox(Center - Extent, Center + Extent);
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FTimeThiefSmokeClusterBroadphaseAutomationTest,
		"TimeThief.Smoke.World.ClusterBroadphase",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FTimeThiefSmokeClusterBroadphaseAutomationTest::RunTest(const FString& Parameters)
	{
		TArray<FBox> FarBounds;
		TArray<FBox> FarReleaseBounds;
		const float CellStride = TimeThiefSmokeParameterDefaults::SmokeSpatialCellSize * 3.0f;
		for (int32 Index = 0; Index < 64; ++Index)
		{
			const FVector Center(CellStride * static_cast<float>(Index), 0.0, 0.0);
			const FBox Bounds = MakeAutomationClusterBox(Center, FVector(80.0, 80.0, 80.0));
			FarBounds.Add(Bounds);
			FarReleaseBounds.Add(Bounds);
		}

		TArray<TPair<int32, int32>> CandidatePairs;
		BuildSmokeClusterCandidatePairs(FarBounds, FarReleaseBounds, CandidatePairs);
		TestEqual(TEXT("Separated smoke cluster broadphase emits no candidate pairs"), CandidatePairs.Num(), 0);

		TArray<FBox> TouchingBounds;
		TArray<FBox> TouchingReleaseBounds;
		TouchingBounds.Add(MakeAutomationClusterBox(FVector::ZeroVector, FVector(200.0, 200.0, 200.0)));
		TouchingBounds.Add(MakeAutomationClusterBox(FVector(320.0, 0.0, 0.0), FVector(200.0, 200.0, 200.0)));
		TouchingReleaseBounds = TouchingBounds;

		CandidatePairs.Reset();
		BuildSmokeClusterCandidatePairs(TouchingBounds, TouchingReleaseBounds, CandidatePairs);
		TestEqual(TEXT("Overlapping smoke cluster broadphase emits one candidate pair"), CandidatePairs.Num(), 1);
		if (CandidatePairs.Num() == 1)
		{
			TestEqual(TEXT("Overlapping pair uses first smoke index"), CandidatePairs[0].Key, 0);
			TestEqual(TEXT("Overlapping pair uses second smoke index"), CandidatePairs[0].Value, 1);
		}

		return true;
	}
#endif
}

void UTimeThiefSmokeWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UTimeThiefSmokeWorldSubsystem::Deinitialize()
{
	ActiveSmokeVolumes.Reset();
	ActiveImpulses.Reset();
	PendingRendererEvents.Reset();
	PersistentClusterLinks.Reset();
	SmokeSpatialEntries.Reset();
	SmokeSpatialCells.Reset();
	bSmokeSpatialIndexDirty = true;
	SmokeSpatialIndexFrame = MAX_uint64;
	Super::Deinitialize();
}

void UTimeThiefSmokeWorldSubsystem::Tick(float DeltaTime)
{
	CompactSmokeVolumes();

	for (int32 Index = ActiveImpulses.Num() - 1; Index >= 0; --Index)
	{
		FTimeThiefActiveSmokeImpulse& ActiveImpulse = ActiveImpulses[Index];
		ATimeThiefSmokeVolume* SmokeVolume = ActiveImpulse.SmokeVolume.Get();
		if (!SmokeVolume || ActiveImpulse.Duration <= KINDA_SMALL_NUMBER)
		{
			ActiveImpulses.RemoveAtSwap(Index);
			continue;
		}

		ActiveImpulse.Age += DeltaTime;
		const float NormalizedAge = FMath::Clamp(ActiveImpulse.Age / ActiveImpulse.Duration, 0.0f, 1.0f);
		const float StrengthScale = 1.0f - (NormalizedAge * NormalizedAge * (3.0f - 2.0f * NormalizedAge));

		if (NormalizedAge >= 1.0f || StrengthScale <= KINDA_SMALL_NUMBER)
		{
			ActiveImpulses.RemoveAtSwap(Index);
			continue;
		}

		FTimeThiefSmokeInteractionEvent Event = ActiveImpulse.Event;
		Event.NormalizedAge = NormalizedAge;
		Event.Strength *= StrengthScale;

		SmokeVolume->ApplyInteractionEvent(Event);
	}

	PublishRendererFrame(DeltaTime);

	BulletTraceCountsThisTick.Reset();
	bSmokeSpatialIndexDirty = true;
}

TStatId UTimeThiefSmokeWorldSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTimeThiefSmokeWorldSubsystem, STATGROUP_Tickables);
}

void UTimeThiefSmokeWorldSubsystem::RegisterSmokeVolume(ATimeThiefSmokeVolume* SmokeVolume)
{
	if (!SmokeVolume)
	{
		return;
	}

	ActiveSmokeVolumes.AddUnique(SmokeVolume);
	MarkSmokeSpatialIndexDirty();
}

void UTimeThiefSmokeWorldSubsystem::UnregisterSmokeVolume(ATimeThiefSmokeVolume* SmokeVolume)
{
	ActiveSmokeVolumes.Remove(SmokeVolume);
	MarkSmokeSpatialIndexDirty();

	for (int32 Index = ActiveImpulses.Num() - 1; Index >= 0; --Index)
	{
		if (ActiveImpulses[Index].SmokeVolume.Get() == SmokeVolume)
		{
			ActiveImpulses.RemoveAtSwap(Index);
		}
	}
}

void UTimeThiefSmokeWorldSubsystem::SubmitBulletTrace(const FVector& TraceStart, const FVector& TraceEnd, float Strength, int32 Seed)
{
	if (TraceStart.Equals(TraceEnd))
	{
		return;
	}

	CompactSmokeVolumes();

	TArray<ATimeThiefSmokeVolume*> CandidateSmokeVolumes;
	QuerySmokeSpatialIndex(TimeThiefSmoke::MakeTraceQueryBounds(TraceStart, TraceEnd), CandidateSmokeVolumes);
	for (ATimeThiefSmokeVolume* SmokeVolume : CandidateSmokeVolumes)
	{
		if (!SmokeVolume)
		{
			continue;
		}

		FVector EntryPoint = FVector::ZeroVector;
		FVector ExitPoint = FVector::ZeroVector;
		if (SmokeVolume->IntersectTraceSegment(TraceStart, TraceEnd, EntryPoint, ExitPoint))
		{
			int32& TraceCount = BulletTraceCountsThisTick.FindOrAdd(SmokeVolume);
			if (TraceCount >= TimeThiefSmokeParameterDefaults::MaxBulletTracesPerSmokePerTick)
			{
				continue;
			}

			++TraceCount;
			SmokeVolume->HandleBulletTrace(EntryPoint, ExitPoint, Strength, Seed);
		}
	}
}

void UTimeThiefSmokeWorldSubsystem::SubmitExplosion(const FVector& Center, float Radius, float Strength, int32 Seed)
{
	CompactSmokeVolumes();

	const float SafeRadius = FMath::Max(1.0f, Radius);
	TArray<ATimeThiefSmokeVolume*> CandidateSmokeVolumes;
	QuerySmokeSpatialIndex(FBox(Center - FVector(SafeRadius), Center + FVector(SafeRadius)), CandidateSmokeVolumes);
	for (ATimeThiefSmokeVolume* SmokeVolume : CandidateSmokeVolumes)
	{
		if (!SmokeVolume || !SmokeVolume->IntersectsExplosion(Center, SafeRadius))
		{
			continue;
		}

		SmokeVolume->HandleExplosionShock(Center, SafeRadius, Strength, Seed);
	}
}

void UTimeThiefSmokeWorldSubsystem::AddTimedInteractionEvent(ATimeThiefSmokeVolume* SmokeVolume, const FTimeThiefSmokeInteractionEvent& Event, float Duration)
{
	if (!SmokeVolume)
	{
		return;
	}

	FTimeThiefActiveSmokeImpulse ActiveImpulse;
	ActiveImpulse.SmokeVolume = SmokeVolume;
	ActiveImpulse.Event = Event;
	ActiveImpulse.Age = 0.0f;
	ActiveImpulse.Duration = FMath::Max(TimeThiefSmokeParameterDefaults::ActiveImpulseMinDurationSeconds, Duration);
	if (TimeThiefSmoke::AddPrioritizedActiveImpulse(ActiveImpulses, ActiveImpulse))
	{
		SmokeVolume->ApplyInteractionEvent(Event);
	}
}

void UTimeThiefSmokeWorldSubsystem::RecordRendererEvent(const FTimeThiefSmokeInteractionEvent& Event)
{
	PendingRendererEvents.Add(Event);
}

void UTimeThiefSmokeWorldSubsystem::CompactSmokeVolumes()
{
	bool bRemovedInvalidVolume = false;
	for (int32 Index = ActiveSmokeVolumes.Num() - 1; Index >= 0; --Index)
	{
		if (!ActiveSmokeVolumes[Index].IsValid())
		{
			ActiveSmokeVolumes.RemoveAtSwap(Index);
			bRemovedInvalidVolume = true;
		}
	}

	if (bRemovedInvalidVolume)
	{
		MarkSmokeSpatialIndexDirty();
	}
}

void UTimeThiefSmokeWorldSubsystem::MarkSmokeSpatialIndexDirty()
{
	bSmokeSpatialIndexDirty = true;
}

void UTimeThiefSmokeWorldSubsystem::RebuildSmokeSpatialIndex()
{
	const uint64 CurrentFrame = GFrameCounter;
	if (!bSmokeSpatialIndexDirty && SmokeSpatialIndexFrame == CurrentFrame)
	{
		return;
	}

	SmokeSpatialEntries.Reset();
	SmokeSpatialCells.Reset();

	for (const TWeakObjectPtr<ATimeThiefSmokeVolume>& SmokeVolumePtr : ActiveSmokeVolumes)
	{
		ATimeThiefSmokeVolume* SmokeVolume = SmokeVolumePtr.Get();
		if (!SmokeVolume)
		{
			continue;
		}

		const FBox Bounds = SmokeVolume->GetCurrentSmokeWorldBounds();
		if (!Bounds.IsValid)
		{
			continue;
		}

		const int32 EntryIndex = SmokeSpatialEntries.Add({ SmokeVolume, Bounds });
		const FIntVector MinCell = TimeThiefSmoke::WorldToSmokeSpatialCell(Bounds.Min);
		const FIntVector MaxCell = TimeThiefSmoke::WorldToSmokeSpatialCell(Bounds.Max);
		for (int32 Z = MinCell.Z; Z <= MaxCell.Z; ++Z)
		{
			for (int32 Y = MinCell.Y; Y <= MaxCell.Y; ++Y)
			{
				for (int32 X = MinCell.X; X <= MaxCell.X; ++X)
				{
					SmokeSpatialCells.FindOrAdd(FIntVector(X, Y, Z)).Add(EntryIndex);
				}
			}
		}
	}

	SmokeSpatialIndexFrame = CurrentFrame;
	bSmokeSpatialIndexDirty = false;
}

void UTimeThiefSmokeWorldSubsystem::QuerySmokeSpatialIndex(const FBox& QueryBounds, TArray<ATimeThiefSmokeVolume*>& OutSmokeVolumes)
{
	OutSmokeVolumes.Reset();
	if (!QueryBounds.IsValid)
	{
		return;
	}

	RebuildSmokeSpatialIndex();

	TSet<int32> CandidateEntryIndices;
	const FIntVector MinCell = TimeThiefSmoke::WorldToSmokeSpatialCell(QueryBounds.Min);
	const FIntVector MaxCell = TimeThiefSmoke::WorldToSmokeSpatialCell(QueryBounds.Max);
	for (int32 Z = MinCell.Z; Z <= MaxCell.Z; ++Z)
	{
		for (int32 Y = MinCell.Y; Y <= MaxCell.Y; ++Y)
		{
			for (int32 X = MinCell.X; X <= MaxCell.X; ++X)
			{
				if (const TArray<int32>* EntryIndices = SmokeSpatialCells.Find(FIntVector(X, Y, Z)))
				{
					for (const int32 EntryIndex : *EntryIndices)
					{
						CandidateEntryIndices.Add(EntryIndex);
					}
				}
			}
		}
	}

	TArray<int32> SortedCandidateEntryIndices = CandidateEntryIndices.Array();
	SortedCandidateEntryIndices.Sort();
	for (const int32 EntryIndex : SortedCandidateEntryIndices)
	{
		if (!SmokeSpatialEntries.IsValidIndex(EntryIndex) || !SmokeSpatialEntries[EntryIndex].Bounds.Intersect(QueryBounds))
		{
			continue;
		}

		if (ATimeThiefSmokeVolume* SmokeVolume = SmokeSpatialEntries[EntryIndex].SmokeVolume.Get())
		{
			OutSmokeVolumes.Add(SmokeVolume);
		}
	}
}

void UTimeThiefSmokeWorldSubsystem::PublishRendererFrame(float DeltaTime)
{
	if (!GEngine)
	{
		PendingRendererEvents.Reset();
		return;
	}

	UTimeThiefSmokeRendererSubsystem* RendererSubsystem = GEngine->GetEngineSubsystem<UTimeThiefSmokeRendererSubsystem>();
	if (!RendererSubsystem)
	{
		PendingRendererEvents.Reset();
		return;
	}

	FTimeThiefSmokeRendererFrame Frame;
	Frame.DeltaSeconds = DeltaTime;
	Frame.Volumes.Reserve(ActiveSmokeVolumes.Num());
	Frame.Events.Reserve(PendingRendererEvents.Num());

	TArray<TimeThiefSmoke::FPendingRendererSmokeVolume> PendingVolumes;
	PendingVolumes.Reserve(ActiveSmokeVolumes.Num());
	for (const TWeakObjectPtr<ATimeThiefSmokeVolume>& SmokeVolumePtr : ActiveSmokeVolumes)
	{
		ATimeThiefSmokeVolume* SmokeVolume = SmokeVolumePtr.Get();
		if (!SmokeVolume)
		{
			continue;
		}
		SmokeVolume->FlushPendingObstacleFieldRebuild();

		FTimeThiefSmokeRendererVolume RendererVolume;
		RendererVolume.SmokeId = SmokeVolume->GetSmokeId();
		RendererVolume.LocalToWorld = FTransform3f(SmokeVolume->GetActorTransform());
		RendererVolume.NaturalBoundsExtent = FVector3f(SmokeVolume->GetCurrentSmokeBoundsExtent());
		RendererVolume.RenderBoundsExtent = FVector3f(SmokeVolume->GetCurrentSmokeRenderBoundsExtent());
		RendererVolume.SimulationBoundsExtent = FVector3f(
			FMath::Max(RendererVolume.RenderBoundsExtent.X, RendererVolume.NaturalBoundsExtent.X),
			FMath::Max(RendererVolume.RenderBoundsExtent.Y, RendererVolume.NaturalBoundsExtent.Y),
			FMath::Max(RendererVolume.RenderBoundsExtent.Z, RendererVolume.NaturalBoundsExtent.Z));
		RendererVolume.BoundsExtent = RendererVolume.SimulationBoundsExtent;
		RendererVolume.AgeSeconds = SmokeVolume->GetSmokeAgeSeconds();
		RendererVolume.DurationSeconds = TimeThiefSmokeParameterDefaults::SmokeDuration;
		RendererVolume.ObstacleFieldResolution = SmokeVolume->GetObstacleFieldResolution();
		RendererVolume.ObstacleFieldRevision = SmokeVolume->GetObstacleFieldRevision();
		RendererVolume.ObstaclePrimitives = SmokeVolume->GetObstaclePrimitives();
		RendererVolume.bHasSolidObstacleField = SmokeVolume->HasSolidObstacleField();
		RendererVolume.Settings = TimeThiefSmoke::GetDefaultRendererSettings();

		TimeThiefSmoke::FPendingRendererSmokeVolume& PendingVolume = PendingVolumes.AddDefaulted_GetRef();
		PendingVolume.Volume = MoveTemp(RendererVolume);
		PendingVolume.ClusterBounds = TimeThiefSmoke::MakeSmokeClusterWorldBounds(PendingVolume.Volume);
	}

	TimeThiefSmoke::AssignSmokeClusters(PendingVolumes, PersistentClusterLinks);
	TMap<int32, TArray<TimeThiefSmoke::FPendingRendererSmokeVolume*>> ClusterMembersById;
	TMap<int32, int32> SmokeIdToPublishedSmokeId;
	for (TimeThiefSmoke::FPendingRendererSmokeVolume& PendingVolume : PendingVolumes)
	{
		ClusterMembersById.FindOrAdd(PendingVolume.Volume.ClusterId).Add(&PendingVolume);
		SmokeIdToPublishedSmokeId.Add(PendingVolume.Volume.SmokeId, PendingVolume.Volume.ClusterId);
	}

	for (TPair<int32, TArray<TimeThiefSmoke::FPendingRendererSmokeVolume*>>& ClusterPair : ClusterMembersById)
	{
		TArray<TimeThiefSmoke::FPendingRendererSmokeVolume*>& ClusterMembers = ClusterPair.Value;
		if (ClusterMembers.Num() <= 1)
		{
			TimeThiefSmoke::FPendingRendererSmokeVolume* Member = ClusterMembers.IsEmpty() ? nullptr : ClusterMembers[0];
			if (Member)
			{
				Member->Volume.SourceEvents.Reset();
				Frame.Volumes.Add(MoveTemp(Member->Volume));
			}
			continue;
		}

		Frame.Volumes.Add(TimeThiefSmoke::MakeClusterVolume(ClusterMembers));
	}

	for (const FTimeThiefSmokeInteractionEvent& Event : PendingRendererEvents)
	{
		FTimeThiefSmokeRendererEvent RendererEvent = TimeThiefSmoke::ToRendererEvent(Event);
		if (const int32* PublishedSmokeId = SmokeIdToPublishedSmokeId.Find(RendererEvent.SmokeId))
		{
			RendererEvent.SmokeId = *PublishedSmokeId;
		}
		Frame.Events.Add(RendererEvent);
	}

	RendererSubsystem->SubmitFrame(Frame);
	PendingRendererEvents.Reset();
}
