#include "Smoke/TimeThiefSmokeWorldSubsystem.h"

#include "Actors/TimeThiefSmokeVolume.h"
#include "Engine/Engine.h"
#include "Stats/Stats.h"
#include "TimeThiefSmokeParameterDefaults.h"
#include "TimeThiefSmokeRendererSubsystem.h"

namespace TimeThiefSmoke
{
	constexpr int32 MaxBulletTracesPerSmokePerTick = TimeThiefSmokeParameterDefaults::MaxBulletTracesPerSmokePerTick;
	constexpr int32 MaxActiveExplosionImpulsesPerSmoke = TimeThiefSmokeParameterDefaults::MaxActiveExplosionImpulsesPerSmoke;
	constexpr float SmokeClusterBoundsExpansionRatio = TimeThiefSmokeParameterDefaults::SmokeClusterBoundsExpansionRatio;
	constexpr float SmokeClusterMinExpansionCm = TimeThiefSmokeParameterDefaults::SmokeClusterMinExpansionCm;
	constexpr float SmokeClusterReleaseBoundsExpansionRatio = TimeThiefSmokeParameterDefaults::SmokeClusterReleaseBoundsExpansionRatio;
	constexpr float SmokeClusterReleaseMinExpansionCm = TimeThiefSmokeParameterDefaults::SmokeClusterReleaseMinExpansionCm;
	constexpr float SmokeSpatialCellSize = TimeThiefSmokeParameterDefaults::SmokeSpatialCellSize;

	struct FPendingRendererSmokeVolume
	{
		FTimeThiefSmokeRendererVolume Volume;
		FBox ClusterBounds = FBox(EForceInit::ForceInit);
	};

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

	bool HasSolidObstacleMask(const FTimeThiefSmokeRendererVolume& Volume)
	{
		return Volume.bHasSolidObstacleMask;
	}

	FIntVector WorldToSmokeSpatialCell(const FVector& Position)
	{
		const float SafeCellSize = FMath::Max(SmokeSpatialCellSize, 1.0f);
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
		return A.Settings.SimulationBackend == ETimeThiefSmokeSimulationBackend::SparseMac &&
			B.Settings.SimulationBackend == ETimeThiefSmokeSimulationBackend::SparseMac &&
			A.Settings.PressureSolver == B.Settings.PressureSolver &&
			A.Settings.SmokeGridResolution == B.Settings.SmokeGridResolution &&
			A.Settings.SmokeBrickSize == B.Settings.SmokeBrickSize &&
			A.Settings.MaxActiveSmokeBricks == B.Settings.MaxActiveSmokeBricks &&
			!HasSolidObstacleMask(A) &&
			!HasSolidObstacleMask(B);
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
			ExpandedClusterBounds.Add(ExpandSmokeClusterBounds(PendingVolume.ClusterBounds, SmokeClusterBoundsExpansionRatio, SmokeClusterMinExpansionCm));
			ReleaseClusterBounds.Add(ExpandSmokeClusterBounds(PendingVolume.ClusterBounds, SmokeClusterReleaseBoundsExpansionRatio, SmokeClusterReleaseMinExpansionCm));
		}

		TSet<uint64> NextPersistentClusterLinks;
		for (int32 A = 0; A < PendingVolumes.Num(); ++A)
		{
			for (int32 B = A + 1; B < PendingVolumes.Num(); ++B)
			{
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

	FTimeThiefSmokeRendererEvent MakeClusterSourceEvent(const FTimeThiefSmokeRendererVolume& Volume, const int32 ClusterSmokeId)
	{
		FTimeThiefSmokeRendererEvent SourceEvent;
		SourceEvent.SmokeId = ClusterSmokeId;
		SourceEvent.Type = ETimeThiefSmokeRendererInteractionType::PlumeSource;
		SourceEvent.Shape = ETimeThiefSmokeRendererInteractionShape::Sphere;
		SourceEvent.Position = Volume.LocalToWorld.GetLocation();
		SourceEvent.PreviousPosition = SourceEvent.Position;
		SourceEvent.Direction = Volume.LocalToWorld.TransformVector(FVector3f(0.0f, 0.0f, 1.0f)).GetSafeNormal();
		SourceEvent.Radius = FMath::Max(Volume.Settings.PlumeSourceRadius, 1.0f);
		SourceEvent.Length = SourceEvent.Radius * 2.0f;
		SourceEvent.NormalizedAge = Volume.Settings.PlumeEmissionDuration > KINDA_SMALL_NUMBER
			? Volume.AgeSeconds / Volume.Settings.PlumeEmissionDuration
			: 1.0f;
		SourceEvent.Strength = SourceEvent.NormalizedAge <= 1.25f ? 1.0f : 0.0f;
		SourceEvent.Seed = Volume.SmokeId;
		return SourceEvent;
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
		ClusterVolume.ObstacleMaskResolution = 0;
		ClusterVolume.ObstacleMaskRevision = 0;
		ClusterVolume.ObstacleMask.Reset();
		ClusterVolume.bHasSolidObstacleMask = false;
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

		for (const FPendingRendererSmokeVolume* Member : ClusterMembers)
		{
			ClusterVolume.SourceEvents.Add(MakeClusterSourceEvent(Member->Volume, ClusterVolume.SmokeId));
		}

		return ClusterVolume;
	}

	float ComputeInteractionEventPriority(const FTimeThiefSmokeInteractionEvent& Event, const float Age, const float Duration)
	{
		const float NormalizedAge = Duration > KINDA_SMALL_NUMBER ? FMath::Clamp(Age / Duration, 0.0f, 1.0f) : 1.0f;
		const float AgeWeight = FMath::Max(1.0f - NormalizedAge, 0.05f);
		const float Strength = FMath::Max(Event.Strength, 0.01f);
		const float RadiusWeight = FMath::Clamp(Event.Radius / 200.0f, 0.5f, 4.0f);
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

		if (MatchingCount < MaxActiveExplosionImpulsesPerSmoke)
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
			if (TraceCount >= TimeThiefSmoke::MaxBulletTracesPerSmokePerTick)
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
	ActiveImpulse.Duration = FMath::Max(0.01f, Duration);
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
		SmokeVolume->FlushPendingObstacleMaskRebuild();

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
		RendererVolume.ObstacleMaskResolution = SmokeVolume->GetObstacleMaskResolution();
		RendererVolume.ObstacleMaskRevision = SmokeVolume->GetObstacleMaskRevision();
		RendererVolume.ObstacleMask = SmokeVolume->GetObstacleMaskSnapshot();
		RendererVolume.bHasSolidObstacleMask = SmokeVolume->HasSolidObstacleMask();
		RendererVolume.Settings = FTimeThiefSmokeRendererSettings();

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
