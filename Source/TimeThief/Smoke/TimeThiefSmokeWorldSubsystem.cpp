#include "Smoke/TimeThiefSmokeWorldSubsystem.h"

#include "Actors/TimeThiefSmokeVolume.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Stats/Stats.h"
#include "TimeThiefSmokeParameterDefaults.h"
#include "TimeThiefSmokeRendererSubsystem.h"
#include "WorldCollision.h"

namespace TimeThiefSmoke
{
	struct FSmokeActorOverlapGroup
	{
		TArray<int32> SmokeIndices;
		FBox Bounds = FBox(EForceInit::ForceInit);
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

	uint64 MakeSmokeActorOverlapPairKey(const int32 A, const int32 B)
	{
		const uint32 Low = static_cast<uint32>(FMath::Min(A, B));
		const uint32 High = static_cast<uint32>(FMath::Max(A, B));
		return (static_cast<uint64>(Low) << 32) | static_cast<uint64>(High);
	}

	struct FSmokeActorOverlapUnionFind
	{
		TArray<int32> Parents;

		explicit FSmokeActorOverlapUnionFind(const int32 Count)
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

	void BuildSmokeActorOverlapGroupsDense(const TArray<FBox>& SmokeBounds, TArray<FSmokeActorOverlapGroup>& OutGroups)
	{
		OutGroups.Reset();
		OutGroups.Reserve(SmokeBounds.Num());
		TArray<bool> bVisited;
		bVisited.Init(false, SmokeBounds.Num());
		TArray<int32> Stack;
		Stack.Reserve(SmokeBounds.Num());

		for (int32 SmokeIndex = 0; SmokeIndex < SmokeBounds.Num(); ++SmokeIndex)
		{
			if (bVisited[SmokeIndex] || !SmokeBounds[SmokeIndex].IsValid)
			{
				continue;
			}

			FSmokeActorOverlapGroup& Group = OutGroups.AddDefaulted_GetRef();
			Stack.Reset();
			Stack.Add(SmokeIndex);
			bVisited[SmokeIndex] = true;

			while (!Stack.IsEmpty())
			{
				const int32 CurrentIndex = Stack.Pop(EAllowShrinking::No);
				Group.SmokeIndices.Add(CurrentIndex);
				Group.Bounds += SmokeBounds[CurrentIndex];

				for (int32 CandidateIndex = 0; CandidateIndex < SmokeBounds.Num(); ++CandidateIndex)
				{
					if (bVisited[CandidateIndex] || !SmokeBounds[CandidateIndex].IsValid)
					{
						continue;
					}

					if (SmokeBounds[CurrentIndex].Intersect(SmokeBounds[CandidateIndex]))
					{
						bVisited[CandidateIndex] = true;
						Stack.Add(CandidateIndex);
					}
				}
			}
		}
	}

	void BuildSmokeActorOverlapGroups(const TArray<FBox>& SmokeBounds, TArray<FSmokeActorOverlapGroup>& OutGroups)
	{
		if (SmokeBounds.Num() <= 8)
		{
			BuildSmokeActorOverlapGroupsDense(SmokeBounds, OutGroups);
			return;
		}

		OutGroups.Reset();
		OutGroups.Reserve(SmokeBounds.Num());

		FSmokeActorOverlapUnionFind Groups(SmokeBounds.Num());
		TMap<FIntVector, TArray<int32>> CellSmokeIndices;
		TSet<uint64> TestedPairKeys;
		CellSmokeIndices.Reserve(SmokeBounds.Num() * 4);
		TestedPairKeys.Reserve(SmokeBounds.Num() * 4);

		for (int32 SmokeIndex = 0; SmokeIndex < SmokeBounds.Num(); ++SmokeIndex)
		{
			if (!SmokeBounds[SmokeIndex].IsValid)
			{
				continue;
			}

			const FIntVector MinCell = WorldToSmokeSpatialCell(SmokeBounds[SmokeIndex].Min);
			const FIntVector MaxCell = WorldToSmokeSpatialCell(SmokeBounds[SmokeIndex].Max);
			for (int32 Z = MinCell.Z; Z <= MaxCell.Z; ++Z)
			{
				for (int32 Y = MinCell.Y; Y <= MaxCell.Y; ++Y)
				{
					for (int32 X = MinCell.X; X <= MaxCell.X; ++X)
					{
						TArray<int32>& CellIndices = CellSmokeIndices.FindOrAdd(FIntVector(X, Y, Z));
						for (const int32 OtherSmokeIndex : CellIndices)
						{
							const uint64 PairKey = MakeSmokeActorOverlapPairKey(SmokeIndex, OtherSmokeIndex);
							if (TestedPairKeys.Contains(PairKey))
							{
								continue;
							}

							TestedPairKeys.Add(PairKey);
							if (SmokeBounds[SmokeIndex].Intersect(SmokeBounds[OtherSmokeIndex]))
							{
								Groups.Union(SmokeIndex, OtherSmokeIndex);
							}
						}
						CellIndices.Add(SmokeIndex);
					}
				}
			}
		}

		TMap<int32, int32> GroupIndexByRoot;
		GroupIndexByRoot.Reserve(SmokeBounds.Num());
		for (int32 SmokeIndex = 0; SmokeIndex < SmokeBounds.Num(); ++SmokeIndex)
		{
			if (!SmokeBounds[SmokeIndex].IsValid)
			{
				continue;
			}

			const int32 Root = Groups.Find(SmokeIndex);
			if (Root == INDEX_NONE)
			{
				continue;
			}

			int32* ExistingGroupIndex = GroupIndexByRoot.Find(Root);
			if (!ExistingGroupIndex)
			{
				const int32 NewGroupIndex = OutGroups.Num();
				OutGroups.AddDefaulted();
				GroupIndexByRoot.Add(Root, NewGroupIndex);
				ExistingGroupIndex = GroupIndexByRoot.Find(Root);
			}

			FSmokeActorOverlapGroup& Group = OutGroups[*ExistingGroupIndex];
			Group.SmokeIndices.Add(SmokeIndex);
			Group.Bounds += SmokeBounds[SmokeIndex];
		}
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
	if (GEngine)
	{
		if (UTimeThiefSmokeRendererSubsystem* RendererSubsystem = GEngine->GetEngineSubsystem<UTimeThiefSmokeRendererSubsystem>())
		{
			FTimeThiefSmokeRendererFrame ClearFrame;
			ClearFrame.SceneKey = GetRendererSceneKey();
			RendererSubsystem->SubmitFrame(MoveTemp(ClearFrame));
		}
	}

	ActiveSmokeVolumes.Reset();
	ActiveImpulses.Reset();
	PendingRendererEvents.Reset();
	SmokeSpatialEntries.Reset();
	SmokeSpatialCells.Reset();
	SmokeSpatialQueryEntryStamps.Reset();
	SmokeSpatialQueryEntryIndices.Reset();
	SmokeSpatialQueryResults.Reset();
	LastPublishedObstacleFieldRevisions.Reset();
	bSmokeSpatialIndexDirty = true;
	SmokeSpatialIndexValidationFrame = MAX_uint64;
	SmokeSpatialQueryStamp = 0;
	NextSmokeId = 1;
	RendererSceneKey = 0;
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

	GatherActorPushEvents(DeltaTime);
	PublishRendererFrame(DeltaTime);

	BulletTraceCountsThisTick.Reset();
}

TStatId UTimeThiefSmokeWorldSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTimeThiefSmokeWorldSubsystem, STATGROUP_Tickables);
}

int32 UTimeThiefSmokeWorldSubsystem::AllocateSmokeId()
{
	return NextSmokeId++;
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
	if (SmokeVolume)
	{
		LastPublishedObstacleFieldRevisions.Remove(SmokeVolume->GetSmokeId());
	}
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

	QuerySmokeSpatialIndex(TimeThiefSmoke::MakeTraceQueryBounds(TraceStart, TraceEnd), SmokeSpatialQueryResults);
	for (ATimeThiefSmokeVolume* SmokeVolume : SmokeSpatialQueryResults)
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
	QuerySmokeSpatialIndex(FBox(Center - FVector(SafeRadius), Center + FVector(SafeRadius)), SmokeSpatialQueryResults);
	for (ATimeThiefSmokeVolume* SmokeVolume : SmokeSpatialQueryResults)
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

void UTimeThiefSmokeWorldSubsystem::NotifySmokeVolumeBoundsChanged(ATimeThiefSmokeVolume* SmokeVolume)
{
	if (SmokeVolume && ActiveSmokeVolumes.Contains(SmokeVolume))
	{
		MarkSmokeSpatialIndexDirty();
	}
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

	if (ActiveSmokeVolumes.IsEmpty())
	{
		LastPublishedObstacleFieldRevisions.Reset();
	}
}

void UTimeThiefSmokeWorldSubsystem::MarkSmokeSpatialIndexDirty()
{
	bSmokeSpatialIndexDirty = true;
}

void UTimeThiefSmokeWorldSubsystem::ValidateSmokeSpatialIndexBounds()
{
	if (bSmokeSpatialIndexDirty || SmokeSpatialIndexValidationFrame == GFrameCounter)
	{
		return;
	}

	SmokeSpatialIndexValidationFrame = GFrameCounter;
	int32 CurrentValidSmokeCount = 0;
	for (const TWeakObjectPtr<ATimeThiefSmokeVolume>& SmokeVolumePtr : ActiveSmokeVolumes)
	{
		const ATimeThiefSmokeVolume* SmokeVolume = SmokeVolumePtr.Get();
		if (SmokeVolume && SmokeVolume->GetCurrentSmokeWorldBounds().IsValid)
		{
			++CurrentValidSmokeCount;
		}
	}

	if (CurrentValidSmokeCount != SmokeSpatialEntries.Num())
	{
		MarkSmokeSpatialIndexDirty();
		return;
	}

	for (const FTimeThiefSmokeSpatialEntry& Entry : SmokeSpatialEntries)
	{
		ATimeThiefSmokeVolume* SmokeVolume = Entry.SmokeVolume.Get();
		if (!SmokeVolume)
		{
			MarkSmokeSpatialIndexDirty();
			return;
		}

		const FBox CurrentBounds = SmokeVolume->GetCurrentSmokeWorldBounds();
		if (!CurrentBounds.IsValid ||
			!Entry.Bounds.IsValid ||
			!Entry.Bounds.Min.Equals(CurrentBounds.Min, 0.1) ||
			!Entry.Bounds.Max.Equals(CurrentBounds.Max, 0.1))
		{
			MarkSmokeSpatialIndexDirty();
			return;
		}
	}
}

void UTimeThiefSmokeWorldSubsystem::RebuildSmokeSpatialIndex()
{
	if (!bSmokeSpatialIndexDirty)
	{
		return;
	}

	SmokeSpatialEntries.Reset();
	SmokeSpatialCells.Reset();
	SmokeSpatialEntries.Reserve(ActiveSmokeVolumes.Num());
	SmokeSpatialCells.Reserve(ActiveSmokeVolumes.Num() * 4);

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

	SmokeSpatialQueryEntryStamps.Reset(SmokeSpatialEntries.Num());
	SmokeSpatialQueryEntryStamps.AddZeroed(SmokeSpatialEntries.Num());
	SmokeSpatialQueryStamp = 0;
	bSmokeSpatialIndexDirty = false;
	SmokeSpatialIndexValidationFrame = GFrameCounter;
}

void UTimeThiefSmokeWorldSubsystem::QuerySmokeSpatialIndex(const FBox& QueryBounds, TArray<ATimeThiefSmokeVolume*>& OutSmokeVolumes)
{
	OutSmokeVolumes.Reset();
	if (!QueryBounds.IsValid)
	{
		return;
	}

	ValidateSmokeSpatialIndexBounds();
	RebuildSmokeSpatialIndex();

	if (SmokeSpatialQueryEntryStamps.Num() != SmokeSpatialEntries.Num())
	{
		SmokeSpatialQueryEntryStamps.Reset(SmokeSpatialEntries.Num());
		SmokeSpatialQueryEntryStamps.AddZeroed(SmokeSpatialEntries.Num());
		SmokeSpatialQueryStamp = 0;
	}
	++SmokeSpatialQueryStamp;
	if (SmokeSpatialQueryStamp == 0)
	{
		SmokeSpatialQueryEntryStamps.Reset(SmokeSpatialEntries.Num());
		SmokeSpatialQueryEntryStamps.AddZeroed(SmokeSpatialEntries.Num());
		SmokeSpatialQueryStamp = 1;
	}
	SmokeSpatialQueryEntryIndices.Reset(SmokeSpatialEntries.Num());
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
						if (!SmokeSpatialEntries.IsValidIndex(EntryIndex) || SmokeSpatialQueryEntryStamps[EntryIndex] == SmokeSpatialQueryStamp)
						{
							continue;
						}

						SmokeSpatialQueryEntryStamps[EntryIndex] = SmokeSpatialQueryStamp;
						SmokeSpatialQueryEntryIndices.Add(EntryIndex);
					}
				}
			}
		}
	}

	SmokeSpatialQueryEntryIndices.Sort();
	OutSmokeVolumes.Reserve(SmokeSpatialQueryEntryIndices.Num());
	for (const int32 EntryIndex : SmokeSpatialQueryEntryIndices)
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

void UTimeThiefSmokeWorldSubsystem::GatherActorPushEvents(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || DeltaTime <= 0.0f)
	{
		return;
	}

	CompactSmokeVolumes();
	TArray<ATimeThiefSmokeVolume*> ValidSmokeVolumes;
	TArray<FBox> ValidSmokeBounds;
	ValidSmokeVolumes.Reserve(ActiveSmokeVolumes.Num());
	ValidSmokeBounds.Reserve(ActiveSmokeVolumes.Num());
	for (const TWeakObjectPtr<ATimeThiefSmokeVolume>& SmokeVolumePtr : ActiveSmokeVolumes)
	{
		ATimeThiefSmokeVolume* SmokeVolume = SmokeVolumePtr.Get();
		if (!SmokeVolume)
		{
			continue;
		}

		const FBox SmokeBounds = SmokeVolume->GetCurrentSmokeWorldBounds();
		if (!SmokeBounds.IsValid)
		{
			continue;
		}

		ValidSmokeVolumes.Add(SmokeVolume);
		ValidSmokeBounds.Add(SmokeBounds);
	}

	if (ValidSmokeVolumes.IsEmpty())
	{
		return;
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TimeThiefSmokeActorOverlap), false);
	TArray<TArray<UPrimitiveComponent*>> ComponentsBySmoke;
	TArray<TSet<UPrimitiveComponent*>> AddedComponentsBySmoke;
	ComponentsBySmoke.SetNum(ValidSmokeVolumes.Num());
	AddedComponentsBySmoke.SetNum(ValidSmokeVolumes.Num());

	TArray<TimeThiefSmoke::FSmokeActorOverlapGroup> OverlapGroups;
	TimeThiefSmoke::BuildSmokeActorOverlapGroups(ValidSmokeBounds, OverlapGroups);

	for (const TimeThiefSmoke::FSmokeActorOverlapGroup& OverlapGroup : OverlapGroups)
	{
		if (!OverlapGroup.Bounds.IsValid)
		{
			continue;
		}

		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByObjectType(
			Overlaps,
			OverlapGroup.Bounds.GetCenter(),
			FQuat::Identity,
			ObjectQueryParams,
			FCollisionShape::MakeBox(OverlapGroup.Bounds.GetExtent()),
			QueryParams);

		for (const FOverlapResult& Overlap : Overlaps)
		{
			UPrimitiveComponent* PrimitiveComponent = Overlap.GetComponent();
			if (!PrimitiveComponent)
			{
				continue;
			}

			const FBox ComponentBounds = PrimitiveComponent->Bounds.GetBox();
			for (const int32 SmokeIndex : OverlapGroup.SmokeIndices)
			{
				if (!ValidSmokeVolumes.IsValidIndex(SmokeIndex) ||
					!ComponentBounds.Intersect(ValidSmokeBounds[SmokeIndex]) ||
					AddedComponentsBySmoke[SmokeIndex].Contains(PrimitiveComponent))
				{
					continue;
				}

				AddedComponentsBySmoke[SmokeIndex].Add(PrimitiveComponent);
				ComponentsBySmoke[SmokeIndex].Add(PrimitiveComponent);
			}
		}
	}

	for (int32 SmokeIndex = 0; SmokeIndex < ValidSmokeVolumes.Num(); ++SmokeIndex)
	{
		if (ValidSmokeVolumes[SmokeIndex])
		{
			ValidSmokeVolumes[SmokeIndex]->GatherActorPushEventsFromComponents(ComponentsBySmoke[SmokeIndex], DeltaTime);
		}
	}
}

uint64 UTimeThiefSmokeWorldSubsystem::GetRendererSceneKey() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}

	return World->Scene ? reinterpret_cast<uint64>(World->Scene) : RendererSceneKey;
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
	const uint64 CurrentSceneKey = GetRendererSceneKey();
	if (RendererSceneKey != CurrentSceneKey)
	{
		LastPublishedObstacleFieldRevisions.Reset();
	}
	Frame.SceneKey = CurrentSceneKey;
	RendererSceneKey = Frame.SceneKey;
	Frame.DeltaSeconds = DeltaTime;
	Frame.Volumes.Reserve(ActiveSmokeVolumes.Num());
	Frame.Events.Reserve(PendingRendererEvents.Num());

	for (const TWeakObjectPtr<ATimeThiefSmokeVolume>& SmokeVolumePtr : ActiveSmokeVolumes)
	{
		ATimeThiefSmokeVolume* SmokeVolume = SmokeVolumePtr.Get();
		if (!SmokeVolume)
		{
			continue;
		}
		SmokeVolume->FlushPendingObstacleFieldRebuild(DeltaTime);

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
		RendererVolume.bHasSolidObstacleField = SmokeVolume->HasSolidObstacleField();
		const uint32* LastPublishedObstacleFieldRevision = CurrentSceneKey != 0
			? LastPublishedObstacleFieldRevisions.Find(RendererVolume.SmokeId)
			: nullptr;
		if (!LastPublishedObstacleFieldRevision || *LastPublishedObstacleFieldRevision != RendererVolume.ObstacleFieldRevision)
		{
			RendererVolume.ObstaclePrimitives = SmokeVolume->GetObstaclePrimitives();
			if (CurrentSceneKey != 0)
			{
				LastPublishedObstacleFieldRevisions.Add(RendererVolume.SmokeId, RendererVolume.ObstacleFieldRevision);
			}
		}

		Frame.Volumes.Add(MoveTemp(RendererVolume));
	}

	for (const FTimeThiefSmokeInteractionEvent& Event : PendingRendererEvents)
	{
		Frame.Events.Add(TimeThiefSmoke::ToRendererEvent(Event));
	}

	RendererSubsystem->SubmitFrame(MoveTemp(Frame));
	PendingRendererEvents.Reset();
}
