#include "Actors/TimeThiefSmokeVolume.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SphereComponent.h"
#include "Components/ShapeComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Smoke/TimeThiefSmokeWorldSubsystem.h"
#include "TimeThiefSmokeParameterDefaults.h"
#include "Weapon/TimeThiefRocketProjectile.h"
#include "Weapon/TimeThiefThrowableProjectile.h"
#include "WorldCollision.h"

namespace TimeThiefSmokeVolume
{
	int32 GNextSmokeId = 1;
	const FName ServerCollisionTag(TEXT("ServerCollision"));
	const FName BlockProjectileTag(TEXT("BlockProjectile"));
	const FName BlockMovementTag(TEXT("BlockMovement"));
	const FName IgnoreTag(TEXT("Ignore"));

	bool IsCellCoordValid(const FIntVector& Coord, const FIntVector& CellGrid)
	{
		return Coord.X >= 0 && Coord.Y >= 0 && Coord.Z >= 0 &&
			Coord.X < CellGrid.X && Coord.Y < CellGrid.Y && Coord.Z < CellGrid.Z;
	}

	int32 FlattenCellCoord(const FIntVector& Coord, const FIntVector& CellGrid)
	{
		return Coord.X + Coord.Y * CellGrid.X + Coord.Z * CellGrid.X * CellGrid.Y;
	}

	FVector CellCoordToLocalCenter(const FIntVector& Coord, const FVector& BoundsExtent, const FVector& ClusterOffset, const FIntVector& CellGrid)
	{
		const FVector Alpha(
			(static_cast<float>(Coord.X) + 0.5f) / static_cast<float>(CellGrid.X),
			(static_cast<float>(Coord.Y) + 0.5f) / static_cast<float>(CellGrid.Y),
			(static_cast<float>(Coord.Z) + 0.5f) / static_cast<float>(CellGrid.Z));
		return ((Alpha * 2.0f) - FVector::OneVector) * BoundsExtent + ClusterOffset;
	}

	FVector ClampClusterOffset(const FVector& Offset, const FVector& BoundsExtent)
	{
		const FVector MaxOffset = BoundsExtent * TimeThiefSmokeParameterDefaults::BoundsClusterMaxOffsetRatio;
		return FVector(
			FMath::Clamp(Offset.X, -MaxOffset.X, MaxOffset.X),
			FMath::Clamp(Offset.Y, -MaxOffset.Y, MaxOffset.Y),
			FMath::Clamp(Offset.Z, -MaxOffset.Z, MaxOffset.Z));
	}

	float SmoothStep01(float Alpha)
	{
		const float T = FMath::Clamp(Alpha, 0.0f, 1.0f);
		return T * T * (3.0f - 2.0f * T);
	}

	FVector MakeObstacleMaskQueryExtent(const FVector& CellHalfExtent, float Inflation)
	{
		return FVector(Inflation).ComponentMax(CellHalfExtent * TimeThiefSmokeParameterDefaults::ObstacleMaskCellFootprintRatio);
	}

	FVector MakeBoundsCellObstacleQueryExtent(const FVector& CellHalfExtent)
	{
		return FVector(TimeThiefSmokeParameterDefaults::ObstacleMaskInflation).ComponentMax(CellHalfExtent * TimeThiefSmokeParameterDefaults::BoundsCellObstacleFootprintRatio);
	}

	float GetSourceClearRadius()
	{
		return TimeThiefSmokeParameterDefaults::PlumeSourceRadius * TimeThiefSmokeParameterDefaults::ObstacleSourceClearRadiusScale;
	}

	bool DoesBoxOverlapSourceClearRadius(const FVector& LocalCenter, const FVector& CellHalfExtent)
	{
		const FVector Delta = LocalCenter.GetAbs() - CellHalfExtent;
		const FVector OutsideDelta(
			FMath::Max(Delta.X, 0.0f),
			FMath::Max(Delta.Y, 0.0f),
			FMath::Max(Delta.Z, 0.0f));
		return OutsideDelta.SizeSquared() <= FMath::Square(GetSourceClearRadius());
	}

	bool DoesCellOverlapSourceClearRadius(const FIntVector& Coord, const FVector& BoundsExtent, const FVector& ClusterOffset, const FIntVector& CellGrid)
	{
		const FVector LocalCenter = CellCoordToLocalCenter(Coord, BoundsExtent, ClusterOffset, CellGrid);
		const FVector CellHalfExtent(
			BoundsExtent.X / static_cast<float>(CellGrid.X),
			BoundsExtent.Y / static_cast<float>(CellGrid.Y),
			BoundsExtent.Z / static_cast<float>(CellGrid.Z));
		return DoesBoxOverlapSourceClearRadius(LocalCenter, CellHalfExtent);
	}

	FBox MakeWorldAabbFromOrientedBox(const FVector& Center, const FQuat& Rotation, const FVector& Extent)
	{
		FBox Bounds(EForceInit::ForceInit);
		for (int32 Z = -1; Z <= 1; Z += 2)
		{
			for (int32 Y = -1; Y <= 1; Y += 2)
			{
				for (int32 X = -1; X <= 1; X += 2)
				{
					Bounds += Center + Rotation.RotateVector(FVector(Extent.X * X, Extent.Y * Y, Extent.Z * Z));
				}
			}
		}

		return Bounds;
	}

	bool IsIgnoredSmokeObstacleActor(const AActor* Actor, const ATimeThiefSmokeVolume* SmokeVolume)
	{
		if (!Actor || Actor == SmokeVolume || Actor->IsA<ATimeThiefSmokeVolume>())
		{
			return true;
		}

		if (SmokeVolume && (Actor == SmokeVolume->GetOwner() || Actor == SmokeVolume->GetInstigator()))
		{
			return true;
		}

		return Actor->IsA<APawn>() ||
			Actor->IsA<ATimeThiefThrowableProjectile>() ||
			Actor->IsA<ATimeThiefRocketProjectile>();
	}

	bool IsServerCollisionShape(const UPrimitiveComponent* PrimitiveComponent)
	{
		const AActor* OwnerActor = PrimitiveComponent ? PrimitiveComponent->GetOwner() : nullptr;
		return OwnerActor && OwnerActor->ActorHasTag(ServerCollisionTag) && PrimitiveComponent->IsA<UShapeComponent>();
	}

	bool IsSmokeProjectileObstacleComponent(const UPrimitiveComponent* PrimitiveComponent, const ATimeThiefSmokeVolume* SmokeVolume)
	{
		if (!PrimitiveComponent || PrimitiveComponent->ComponentHasTag(IgnoreTag) || !PrimitiveComponent->IsQueryCollisionEnabled())
		{
			return false;
		}

		const AActor* OwnerActor = PrimitiveComponent->GetOwner();
		if (IsIgnoredSmokeObstacleActor(OwnerActor, SmokeVolume))
		{
			return false;
		}

		const bool bHasProjectileTag = PrimitiveComponent->ComponentHasTag(BlockProjectileTag);
		const bool bHasMovementTag = PrimitiveComponent->ComponentHasTag(BlockMovementTag);
		const bool bIsServerCollisionShape = IsServerCollisionShape(PrimitiveComponent);
		if (bHasProjectileTag || (bIsServerCollisionShape && !bHasMovementTag && !bHasProjectileTag))
		{
			return true;
		}

		return PrimitiveComponent->GetCollisionResponseToChannel(ECC_Visibility) == ECR_Block;
	}

	void AddSmokeObstacleCandidates(const TArray<FOverlapResult>& Overlaps, const ATimeThiefSmokeVolume* SmokeVolume, TArray<TWeakObjectPtr<UPrimitiveComponent>>& OutCandidates)
	{
		for (const FOverlapResult& Overlap : Overlaps)
		{
			UPrimitiveComponent* PrimitiveComponent = Overlap.GetComponent();
			if (IsSmokeProjectileObstacleComponent(PrimitiveComponent, SmokeVolume))
			{
				OutCandidates.AddUnique(PrimitiveComponent);
			}
		}
	}

	void QuerySmokeObstacleCandidates(
		UWorld* World,
		const ATimeThiefSmokeVolume* SmokeVolume,
		const FTransform& SmokeTransform,
		const FVector& BoundsExtent,
		const FVector& QueryExtent,
		const FCollisionQueryParams& QueryParams,
		TArray<TWeakObjectPtr<UPrimitiveComponent>>& OutCandidates)
	{
		if (!World)
		{
			return;
		}

		TArray<FOverlapResult> Overlaps;
		const FCollisionShape BoundsShape = FCollisionShape::MakeBox(BoundsExtent + QueryExtent);
		World->OverlapMultiByChannel(
			Overlaps,
			SmokeTransform.GetLocation(),
			SmokeTransform.GetRotation(),
			ECC_Visibility,
			BoundsShape,
			QueryParams);
		AddSmokeObstacleCandidates(Overlaps, SmokeVolume, OutCandidates);

		FCollisionObjectQueryParams ObjectQueryParams;
		ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
		ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);

		Overlaps.Reset();
		World->OverlapMultiByObjectType(
			Overlaps,
			SmokeTransform.GetLocation(),
			SmokeTransform.GetRotation(),
			ObjectQueryParams,
			BoundsShape,
			QueryParams);
		AddSmokeObstacleCandidates(Overlaps, SmokeVolume, OutCandidates);
	}

	bool AnyCandidateMayOverlap(const TArray<TWeakObjectPtr<UPrimitiveComponent>>& StaticObstacleCandidates, const FBox& QueryBounds)
	{
		for (const TWeakObjectPtr<UPrimitiveComponent>& Candidate : StaticObstacleCandidates)
		{
			const UPrimitiveComponent* PrimitiveComponent = Candidate.Get();
			if (PrimitiveComponent && PrimitiveComponent->Bounds.GetBox().Intersect(QueryBounds))
			{
				return true;
			}
		}

		return false;
	}

	bool AnyCandidateOverlapsBox(
		const TArray<TWeakObjectPtr<UPrimitiveComponent>>& StaticObstacleCandidates,
		const FBox& QueryBounds,
		const FVector& BoxCenter,
		const FQuat& BoxRotation,
		const FCollisionShape& BoxShape)
	{
		for (const TWeakObjectPtr<UPrimitiveComponent>& Candidate : StaticObstacleCandidates)
		{
			const UPrimitiveComponent* PrimitiveComponent = Candidate.Get();
			if (PrimitiveComponent &&
				PrimitiveComponent->Bounds.GetBox().Intersect(QueryBounds) &&
				PrimitiveComponent->OverlapComponent(BoxCenter, BoxRotation, BoxShape))
			{
				return true;
			}
		}

		return false;
	}

}

ATimeThiefSmokeVolume::ATimeThiefSmokeVolume()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	SmokeBoundsComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("SmokeBounds"));
	SetRootComponent(SmokeBoundsComponent);
	SmokeBoundsComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SmokeBoundsComponent->SetCollisionObjectType(ECC_WorldDynamic);
	SmokeBoundsComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	SmokeBoundsComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	SmokeBoundsComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	SmokeBoundsComponent->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	SmokeBoundsComponent->SetGenerateOverlapEvents(false);
}

void ATimeThiefSmokeVolume::InitializeSmokeVolume(AActor* InOwnerActor, APawn* InInstigatorPawn)
{
	if (SmokeId == INDEX_NONE)
	{
		SmokeId = TimeThiefSmokeVolume::GNextSmokeId++;
	}
	SmokeAgeSeconds = 0.0f;
	BoundsClusterLocalOffset = FVector::ZeroVector;
	ActiveBoundsCells.Reset();
	ActiveBoundsCellGrid = FIntVector::ZeroValue;
	PreviousComponentLocations.Reset();
	ActorWarpDensityAccumulations.Reset();

	SetOwner(InOwnerActor);
	SetInstigator(InInstigatorPawn);
	if (SmokeBoundsComponent)
	{
		SmokeBoundsComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	SetLifeSpan(TimeThiefSmokeParameterDefaults::SmokeDuration + TimeThiefSmokeParameterDefaults::SmokeFadeOutDuration);

	UpdateSmokeBounds();
	RebuildStaticObstacleMask();
}

FVector ATimeThiefSmokeVolume::GetCurrentSmokeBoundsExtent() const
{
	return TimeThiefSmokeParameterDefaults::GetSmokeBoundsExtent();
}

FVector ATimeThiefSmokeVolume::GetCurrentSmokeRenderBoundsExtent() const
{
	return GetCurrentSmokeBoundsExtent() + TimeThiefSmokeParameterDefaults::GetRenderBoundsPadding();
}

FBox ATimeThiefSmokeVolume::GetCurrentSmokeWorldBounds() const
{
	const FVector BoundsExtent = GetCurrentSmokeRenderBoundsExtent();
	const FTransform SmokeTransform = GetActorTransform();
	FBox Bounds(EForceInit::ForceInit);

	for (int32 Z = -1; Z <= 1; Z += 2)
	{
		for (int32 Y = -1; Y <= 1; Y += 2)
		{
			for (int32 X = -1; X <= 1; X += 2)
			{
				const FVector LocalCorner(BoundsExtent.X * X, BoundsExtent.Y * Y, BoundsExtent.Z * Z);
				Bounds += SmokeTransform.TransformPosition(LocalCorner);
			}
		}
	}

	return Bounds;
}

TSharedPtr<const TArray<uint8>, ESPMode::ThreadSafe> ATimeThiefSmokeVolume::GetObstacleMaskSnapshot() const
{
	if (ObstacleMaskSnapshotRevision != ObstacleMaskRevision || !ObstacleMaskSnapshot.IsValid())
	{
		TSharedRef<TArray<uint8>, ESPMode::ThreadSafe> NewSnapshot = MakeShared<TArray<uint8>, ESPMode::ThreadSafe>();
		*NewSnapshot = ObstacleMask;
		ObstacleMaskSnapshot = NewSnapshot;
		ObstacleMaskSnapshotRevision = ObstacleMaskRevision;
	}

	return ObstacleMaskSnapshot;
}

void ATimeThiefSmokeVolume::FlushPendingObstacleMaskRebuild()
{
	if (bObstacleMaskRebuildPending)
	{
		RebuildStaticObstacleMask();
	}
}

bool ATimeThiefSmokeVolume::IntersectTraceSegment(const FVector& SegmentStart, const FVector& SegmentEnd, FVector& OutEntryPoint, FVector& OutExitPoint) const
{
	if (!SmokeBoundsComponent)
	{
		return false;
	}

	const FTransform BoundsTransform = SmokeBoundsComponent->GetComponentTransform();
	const FVector LocalStart = BoundsTransform.InverseTransformPosition(SegmentStart);
	const FVector LocalEnd = BoundsTransform.InverseTransformPosition(SegmentEnd);
	const FVector LocalDelta = LocalEnd - LocalStart;
	const FVector Extent = GetCurrentSmokeRenderBoundsExtent();

	float TMin = 0.0f;
	float TMax = 1.0f;

	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const float StartValue = LocalStart[Axis];
		const float DeltaValue = LocalDelta[Axis];
		const float MinValue = -Extent[Axis];
		const float MaxValue = Extent[Axis];

		if (FMath::IsNearlyZero(DeltaValue))
		{
			if (StartValue < MinValue || StartValue > MaxValue)
			{
				return false;
			}
			continue;
		}

		float NearT = (MinValue - StartValue) / DeltaValue;
		float FarT = (MaxValue - StartValue) / DeltaValue;
		if (NearT > FarT)
		{
			Swap(NearT, FarT);
		}

		TMin = FMath::Max(TMin, NearT);
		TMax = FMath::Min(TMax, FarT);
		if (TMin > TMax)
		{
			return false;
		}
	}

	OutEntryPoint = FMath::Lerp(SegmentStart, SegmentEnd, TMin);
	OutExitPoint = FMath::Lerp(SegmentStart, SegmentEnd, TMax);
	return FVector::DistSquared(OutEntryPoint, OutExitPoint) > FMath::Square(1.0f);
}

bool ATimeThiefSmokeVolume::IntersectsExplosion(const FVector& Center, float Radius) const
{
	if (!SmokeBoundsComponent)
	{
		return false;
	}

	const FTransform BoundsTransform = SmokeBoundsComponent->GetComponentTransform();
	const FVector LocalCenter = BoundsTransform.InverseTransformPosition(Center);
	const FVector Extent = GetCurrentSmokeRenderBoundsExtent();
	const FVector ClosestLocal(
		FMath::Clamp(LocalCenter.X, -Extent.X, Extent.X),
		FMath::Clamp(LocalCenter.Y, -Extent.Y, Extent.Y),
		FMath::Clamp(LocalCenter.Z, -Extent.Z, Extent.Z));
	const FVector ClosestWorld = BoundsTransform.TransformPosition(ClosestLocal);
	return FVector::DistSquared(ClosestWorld, Center) <= FMath::Square(FMath::Max(1.0f, Radius));
}

void ATimeThiefSmokeVolume::HandleBulletTrace(const FVector& EntryPoint, const FVector& ExitPoint, float Strength, int32 Seed)
{
	const FVector Segment = ExitPoint - EntryPoint;
	const float SegmentLength = Segment.Size();
	if (SegmentLength <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	FRandomStream RandomStream(Seed ^ (SmokeId * 196613));

	const FVector Direction = Segment / SegmentLength;
	FTimeThiefSmokeInteractionEvent Event;
	Event.SmokeId = SmokeId;
	Event.Type = ESmokeInteractionType::BulletWake;
	Event.Shape = ESmokeInteractionShape::LineWake;
	Event.Position = (EntryPoint + ExitPoint) * 0.5f;
	Event.PreviousPosition = Event.Position;
	Event.Direction = Direction;
	Event.Rotation = Direction.Rotation().Quaternion();
	Event.Radius = TimeThiefSmokeParameterDefaults::BulletClearRadius * RandomStream.FRandRange(TimeThiefSmokeParameterDefaults::BulletClearRadiusRandomMin, TimeThiefSmokeParameterDefaults::BulletClearRadiusRandomMax);
	Event.Length = SegmentLength + Event.Radius * 2.0f;
	Event.Strength = FMath::Clamp(Strength * RandomStream.FRandRange(TimeThiefSmokeParameterDefaults::BulletWakeStrengthRandomMin, TimeThiefSmokeParameterDefaults::BulletWakeStrengthRandomMax), 0.0f, 1.0f);
	Event.NormalizedAge = 0.0f;
	Event.Seed = RandomStream.RandRange(1, INT32_MAX - 1);

	ApplyInteractionEvent(Event);
}

void ATimeThiefSmokeVolume::HandleExplosionShock(const FVector& Center, float Radius, float Strength, int32 Seed)
{
	FTimeThiefSmokeInteractionEvent Event;
	Event.SmokeId = SmokeId;
	Event.Type = ESmokeInteractionType::ExplosionShock;
	Event.Shape = ESmokeInteractionShape::Sphere;
	Event.Position = Center;
	Event.PreviousPosition = Event.Position;
	Event.Direction = (GetActorLocation() - Center).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	Event.Rotation = FQuat::Identity;
	Event.Radius = FMath::Max(Radius, TimeThiefSmokeParameterDefaults::ExplosionShockRadius);
	Event.Length = Event.Radius;
	Event.Strength = FMath::Max(0.0f, Strength);
	Event.Extents = FVector(TimeThiefSmokeParameterDefaults::ExplosionOutwardStrength, TimeThiefSmokeParameterDefaults::ExplosionDensityClearStrength, 0.0f);
	Event.NormalizedAge = 0.0f;
	Event.Seed = Seed;

	if (UTimeThiefSmokeWorldSubsystem* SmokeSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UTimeThiefSmokeWorldSubsystem>() : nullptr)
	{
		SmokeSubsystem->AddTimedInteractionEvent(this, Event, TimeThiefSmokeParameterDefaults::ExplosionImpulseDuration);
	}
	else
	{
		ApplyInteractionEvent(Event);
	}
}

void ATimeThiefSmokeVolume::ApplyInteractionEvent(const FTimeThiefSmokeInteractionEvent& Event)
{
	if (Event.Type == ESmokeInteractionType::ExplosionShock)
	{
		if (Event.NormalizedAge <= KINDA_SMALL_NUMBER)
		{
			ShiftBoundsClusterForExplosion(Event);
		}
	}

	if (UTimeThiefSmokeWorldSubsystem* SmokeSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UTimeThiefSmokeWorldSubsystem>() : nullptr)
	{
		SmokeSubsystem->RecordRendererEvent(Event);
	}
}

void ATimeThiefSmokeVolume::BeginPlay()
{
	Super::BeginPlay();

	if (SmokeBoundsComponent)
	{
		SmokeBoundsComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (SmokeId == INDEX_NONE)
	{
		SmokeId = TimeThiefSmokeVolume::GNextSmokeId++;
	}

	if (UTimeThiefSmokeWorldSubsystem* SmokeSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UTimeThiefSmokeWorldSubsystem>() : nullptr)
	{
		SmokeSubsystem->RegisterSmokeVolume(this);
	}
}

void ATimeThiefSmokeVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UTimeThiefSmokeWorldSubsystem* SmokeSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UTimeThiefSmokeWorldSubsystem>() : nullptr)
	{
		SmokeSubsystem->UnregisterSmokeVolume(this);
	}

	Super::EndPlay(EndPlayReason);
}

void ATimeThiefSmokeVolume::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	SmokeAgeSeconds += FMath::Max(0.0f, DeltaTime);
	FlushPendingObstacleMaskRebuild();
	GatherActorPushEvents(DeltaTime);

	if (TimeThiefSmokeParameterDefaults::bDrawDebugBounds)
	{
		DrawDebugSmoke();
	}
}

void ATimeThiefSmokeVolume::GatherActorPushEvents(float DeltaTime)
{
	if (!GetWorld() || !SmokeBoundsComponent || DeltaTime <= 0.0f)
	{
		return;
	}

	ActorInteractionAccumulator += DeltaTime;
	if (ActorInteractionAccumulator < (1.0f / TimeThiefSmokeParameterDefaults::ActorInteractionHz))
	{
		return;
	}

	const float SampleDeltaTime = ActorInteractionAccumulator;
	ActorInteractionAccumulator = 0.0f;

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TimeThiefSmokeActorOverlap), false);
	QueryParams.AddIgnoredActor(this);
	const FVector ComponentScale = SmokeBoundsComponent->GetComponentScale();
	const FVector AbsComponentScale(FMath::Abs(ComponentScale.X), FMath::Abs(ComponentScale.Y), FMath::Abs(ComponentScale.Z));
	const FVector QueryExtent = GetCurrentSmokeRenderBoundsExtent() * AbsComponentScale;

	TArray<FOverlapResult> Overlaps;
	const bool bAnyOverlap = GetWorld()->OverlapMultiByObjectType(
		Overlaps,
		SmokeBoundsComponent->GetComponentLocation(),
		SmokeBoundsComponent->GetComponentQuat(),
		ObjectQueryParams,
		FCollisionShape::MakeBox(QueryExtent),
		QueryParams);

	if (!bAnyOverlap)
	{
		PreviousComponentLocations.Reset();
		ActorWarpDensityAccumulations.Reset();
		return;
	}

	TSet<TWeakObjectPtr<UPrimitiveComponent>> CurrentDynamicComponents;
	TArray<FTimeThiefSmokeInteractionEvent> ActorEvents;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		UPrimitiveComponent* PrimitiveComponent = Overlap.GetComponent();
		AActor* OverlapOwner = PrimitiveComponent ? PrimitiveComponent->GetOwner() : nullptr;
		if (!PrimitiveComponent ||
			PrimitiveComponent == SmokeBoundsComponent ||
			!OverlapOwner ||
			OverlapOwner == this ||
			OverlapOwner->IsA<ATimeThiefSmokeVolume>() ||
			PrimitiveComponent->Mobility == EComponentMobility::Static)
		{
			continue;
		}

		CurrentDynamicComponents.Add(PrimitiveComponent);

		FTimeThiefSmokeInteractionEvent Event;
		MakeActorPushEvent(PrimitiveComponent, SampleDeltaTime, Event);
		if (Event.Strength <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		ActorEvents.Add(Event);
	}

	for (auto It = PreviousComponentLocations.CreateIterator(); It; ++It)
	{
		const TWeakObjectPtr<UPrimitiveComponent> Component = It.Key();
		if (!Component.IsValid() || !CurrentDynamicComponents.Contains(Component))
		{
			ActorWarpDensityAccumulations.Remove(Component);
			It.RemoveCurrent();
		}
	}

	ActorEvents.Sort([](const FTimeThiefSmokeInteractionEvent& A, const FTimeThiefSmokeInteractionEvent& B)
	{
		return A.Strength > B.Strength;
	});

	const int32 MaxEvents = TimeThiefSmokeParameterDefaults::MaxActorInteractionEventsPerTick;
	for (int32 EventIndex = 0; EventIndex < ActorEvents.Num() && EventIndex < MaxEvents; ++EventIndex)
	{
		const FTimeThiefSmokeInteractionEvent& Event = ActorEvents[EventIndex];
		ApplyInteractionEvent(Event);
	}
}

void ATimeThiefSmokeVolume::MakeActorPushEvent(UPrimitiveComponent* PrimitiveComponent, float DeltaTime, FTimeThiefSmokeInteractionEvent& OutEvent)
{
	OutEvent = FTimeThiefSmokeInteractionEvent();
	OutEvent.Strength = 0.0f;

	if (!PrimitiveComponent)
	{
		return;
	}

	FVector PreviousComponentLocation = PrimitiveComponent->GetComponentLocation();
	const FVector Velocity = ResolveComponentVelocity(PrimitiveComponent, DeltaTime, PreviousComponentLocation);
	const float Speed = Velocity.Size();
	const TWeakObjectPtr<UPrimitiveComponent> ComponentKey(PrimitiveComponent);
	float& WarpAccumulation = ActorWarpDensityAccumulations.FindOrAdd(ComponentKey);
	WarpAccumulation *= FMath::Exp(-DeltaTime / TimeThiefSmokeParameterDefaults::ActorWarpAccumulationDecaySeconds);

	OutEvent.SmokeId = SmokeId;
	OutEvent.Type = ESmokeInteractionType::ActorPush;
	const FVector CurrentBoundsOrigin = PrimitiveComponent->Bounds.Origin;
	const FVector CurrentComponentLocation = PrimitiveComponent->GetComponentLocation();
	const FVector PreviousBoundsOrigin = PreviousComponentLocation + (CurrentBoundsOrigin - CurrentComponentLocation);
	OutEvent.Position = CurrentBoundsOrigin;
	OutEvent.PreviousPosition = PreviousBoundsOrigin;
	OutEvent.Direction = Velocity.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
	OutEvent.Rotation = PrimitiveComponent->GetComponentQuat();
	OutEvent.Speed = Speed;
	OutEvent.NormalizedAge = 0.0f;
	OutEvent.Seed = GetTypeHash(PrimitiveComponent);
	OutEvent.Shape = ResolvePrimitiveShape(PrimitiveComponent, OutEvent);

	const float CurrentDensity = EstimateWarpDensityAtWorldPosition(CurrentBoundsOrigin);
	const float PreviousDensity = EstimateWarpDensityAtWorldPosition(PreviousBoundsOrigin);
	const float MidDensity = EstimateWarpDensityAtWorldPosition((CurrentBoundsOrigin + PreviousBoundsOrigin) * 0.5);
	float DensitySum = CurrentDensity + PreviousDensity + MidDensity;
	float MaxDensity = FMath::Max3(CurrentDensity, PreviousDensity, MidDensity);
	int32 DensitySampleCount = 3;
	const FVector ProbeExtent = PrimitiveComponent->Bounds.BoxExtent * 0.55f;
	const FVector ProbeAxes[3] =
	{
		FVector(ProbeExtent.X, 0.0, 0.0),
		FVector(0.0, ProbeExtent.Y, 0.0),
		FVector(0.0, 0.0, ProbeExtent.Z)
	};
	for (const FVector& ProbeAxis : ProbeAxes)
	{
		if (ProbeAxis.SizeSquared() <= 1.0)
		{
			continue;
		}

		const float PositiveDensity = EstimateWarpDensityAtWorldPosition(CurrentBoundsOrigin + ProbeAxis);
		const float NegativeDensity = EstimateWarpDensityAtWorldPosition(CurrentBoundsOrigin - ProbeAxis);
		DensitySum += PositiveDensity + NegativeDensity;
		MaxDensity = FMath::Max(MaxDensity, FMath::Max(PositiveDensity, NegativeDensity));
		DensitySampleCount += 2;
	}
	const float MeanDensity = DensitySum / static_cast<float>(DensitySampleCount);
	const float PathDensity = FMath::Clamp(FMath::Max(MeanDensity, MaxDensity * TimeThiefSmokeParameterDefaults::ActorPushMaxDensityWeight), 0.0f, 1.0f);
	const float ResponseStartSpeed = TimeThiefSmokeParameterDefaults::ActorPushVelocityThreshold * TimeThiefSmokeParameterDefaults::ActorPushResponseStartSpeedScale;
	const float FullResponseSpeed = TimeThiefSmokeParameterDefaults::ActorPushVelocityThreshold;
	const float ResponseAlpha = TimeThiefSmokeVolume::SmoothStep01((Speed - ResponseStartSpeed) / (FullResponseSpeed - ResponseStartSpeed));
	const float SpeedStrength = FMath::Clamp(Speed / TimeThiefSmokeParameterDefaults::ActorPushFullResponseSpeed, 0.0f, 1.0f);
	const float MotionStrength = FMath::Clamp(ResponseAlpha * SpeedStrength, 0.0f, 1.0f);
	const float OccupancyStrength = PathDensity > TimeThiefSmokeParameterDefaults::ActorPushOccupancyMinDensity
		? FMath::Lerp(TimeThiefSmokeParameterDefaults::ActorPushOccupancyMinStrength, TimeThiefSmokeParameterDefaults::ActorPushOccupancyMaxStrength, PathDensity)
		: 0.0f;
	OutEvent.Strength = FMath::Clamp(FMath::Max(MotionStrength, OccupancyStrength), 0.0f, 1.0f);
	if (OutEvent.Strength <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float TravelDistance = FVector::Dist(CurrentBoundsOrigin, PreviousBoundsOrigin);
	const float TravelGate = TimeThiefSmokeVolume::SmoothStep01(TravelDistance / FMath::Max(OutEvent.Radius * TimeThiefSmokeParameterDefaults::ActorPushTravelGateRadiusScale, TimeThiefSmokeParameterDefaults::ActorPushTravelGateMinDistance));
	const float Deposit = PathDensity * TravelGate * FMath::Lerp(TimeThiefSmokeParameterDefaults::ActorWarpDepositMinStrength, 1.0f, MotionStrength);
	WarpAccumulation = FMath::Clamp(WarpAccumulation + Deposit * TimeThiefSmokeParameterDefaults::ActorWarpDensityAccumulationScale, 0.0f, 1.0f);
	OutEvent.WarpBudget = MotionStrength > 0.01f ? WarpAccumulation : 0.0f;
	WarpAccumulation *= TimeThiefSmokeParameterDefaults::ActorWarpEmissionRemainder;
}

FVector ATimeThiefSmokeVolume::ResolveComponentVelocity(UPrimitiveComponent* PrimitiveComponent, float DeltaTime, FVector& OutPreviousLocation)
{
	if (!PrimitiveComponent)
	{
		OutPreviousLocation = FVector::ZeroVector;
		return FVector::ZeroVector;
	}

	FVector Velocity = PrimitiveComponent->GetComponentVelocity();
	const FVector CurrentLocation = PrimitiveComponent->GetComponentLocation();
	OutPreviousLocation = CurrentLocation;

	if (const FVector* PreviousLocation = PreviousComponentLocations.Find(PrimitiveComponent))
	{
		OutPreviousLocation = *PreviousLocation;
		if (Velocity.IsNearlyZero() && DeltaTime > KINDA_SMALL_NUMBER)
		{
			Velocity = (CurrentLocation - *PreviousLocation) / DeltaTime;
		}
	}

	PreviousComponentLocations.FindOrAdd(PrimitiveComponent) = CurrentLocation;
	return Velocity;
}

float ATimeThiefSmokeVolume::EstimateWarpDensityAtWorldPosition(const FVector& WorldPosition) const
{
	const FTransform SmokeTransform = GetActorTransform();
	const FVector LocalPosition = SmokeTransform.InverseTransformPosition(WorldPosition);
	const FVector NaturalExtent = GetCurrentSmokeBoundsExtent();
	const FVector RenderExtent = GetCurrentSmokeRenderBoundsExtent();
	const FVector NaturalNormalized = LocalPosition / NaturalExtent;
	const FVector RenderNormalized = LocalPosition / RenderExtent;
	const float NaturalDistance = FVector(NaturalNormalized.X, NaturalNormalized.Y, NaturalNormalized.Z / TimeThiefSmokeParameterDefaults::ActorDensityNaturalVerticalScale).Size();
	const float RenderDistance = FVector(RenderNormalized.X, RenderNormalized.Y, RenderNormalized.Z / TimeThiefSmokeParameterDefaults::ActorDensityRenderVerticalScale).Size();
	const float NaturalDensity = 1.0f - TimeThiefSmokeVolume::SmoothStep01((NaturalDistance - TimeThiefSmokeParameterDefaults::ActorDensityNaturalFadeStart) / TimeThiefSmokeParameterDefaults::ActorDensityNaturalFadeWidth);
	const float RenderFade = 1.0f - TimeThiefSmokeVolume::SmoothStep01((RenderDistance - TimeThiefSmokeParameterDefaults::ActorDensityRenderFadeStart) / TimeThiefSmokeParameterDefaults::ActorDensityRenderFadeWidth);
	const float EmissionAlpha = TimeThiefSmokeVolume::SmoothStep01(SmokeAgeSeconds / TimeThiefSmokeParameterDefaults::ActorDensityEmissionWarmupSeconds);
	const float LifetimeAlpha = 1.0f - TimeThiefSmokeVolume::SmoothStep01((SmokeAgeSeconds - TimeThiefSmokeParameterDefaults::SmokeDuration) / TimeThiefSmokeParameterDefaults::SmokeFadeOutDuration);
	const float DensityScale = FMath::Clamp(TimeThiefSmokeParameterDefaults::InitialDensity / TimeThiefSmokeParameterDefaults::ActorDensityScaleDivisor, 0.0f, 1.0f);
	return FMath::Clamp(NaturalDensity * RenderFade * EmissionAlpha * LifetimeAlpha * DensityScale * TimeThiefSmokeParameterDefaults::ActorDensitySampleBoost, 0.0f, 1.0f);
}

ESmokeInteractionShape ATimeThiefSmokeVolume::ResolvePrimitiveShape(UPrimitiveComponent* PrimitiveComponent, FTimeThiefSmokeInteractionEvent& OutEvent) const
{
	if (const USphereComponent* SphereComponent = Cast<USphereComponent>(PrimitiveComponent))
	{
		OutEvent.Radius = SphereComponent->GetScaledSphereRadius();
		OutEvent.Length = 0.0f;
		OutEvent.Extents = FVector(OutEvent.Radius);
		return ESmokeInteractionShape::Sphere;
	}

	if (const UCapsuleComponent* CapsuleComponent = Cast<UCapsuleComponent>(PrimitiveComponent))
	{
		OutEvent.Radius = CapsuleComponent->GetScaledCapsuleRadius();
		OutEvent.Length = CapsuleComponent->GetScaledCapsuleHalfHeight() * 2.0f;
		OutEvent.Extents = FVector(OutEvent.Radius, OutEvent.Radius, OutEvent.Length * 0.5f);
		return ESmokeInteractionShape::Capsule;
	}

	if (const UBoxComponent* BoxComponent = Cast<UBoxComponent>(PrimitiveComponent))
	{
		OutEvent.Extents = BoxComponent->GetScaledBoxExtent();
		OutEvent.Radius = OutEvent.Extents.GetMax() * TimeThiefSmokeParameterDefaults::ActorPrimitiveRadiusScale;
		OutEvent.Length = OutEvent.Extents.GetMax();
		return ESmokeInteractionShape::Box;
	}

	OutEvent.Extents = PrimitiveComponent->Bounds.BoxExtent;
	OutEvent.Radius = OutEvent.Extents.GetMax() * TimeThiefSmokeParameterDefaults::ActorPrimitiveRadiusScale;
	OutEvent.Length = OutEvent.Extents.GetMax();
	return ESmokeInteractionShape::Box;
}

void ATimeThiefSmokeVolume::ShiftBoundsClusterForExplosion(const FTimeThiefSmokeInteractionEvent& Event)
{
	if (!TimeThiefSmokeParameterDefaults::bUseBoundsCellCluster)
	{
		return;
	}

	const FVector WorldDirection = (GetActorLocation() - Event.Position).GetSafeNormal(UE_SMALL_NUMBER, Event.Direction.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector));
	const float ShiftDistance = Event.Extents.X * TimeThiefSmokeParameterDefaults::ExplosionImpulseDuration * TimeThiefSmokeParameterDefaults::ExplosionBoundsShiftScale * Event.Strength;
	const FVector LocalShift = GetActorTransform().InverseTransformVectorNoScale(WorldDirection * ShiftDistance);
	const FVector PreviousOffset = BoundsClusterLocalOffset;
	BoundsClusterLocalOffset = TimeThiefSmokeVolume::ClampClusterOffset(BoundsClusterLocalOffset + LocalShift, GetCurrentSmokeBoundsExtent());
	if (!BoundsClusterLocalOffset.Equals(PreviousOffset, 1.0f))
	{
		MarkObstacleMaskDirty();
	}
}

void ATimeThiefSmokeVolume::MarkObstacleMaskDirty()
{
	bObstacleMaskRebuildPending = true;
}

void ATimeThiefSmokeVolume::RebuildStaticObstacleMask()
{
	bObstacleMaskRebuildPending = false;
	ObstacleMask.Reset();
	ObstacleMaskResolution = 0;
	ActiveBoundsCells.Reset();
	ActiveBoundsCellGrid = FIntVector::ZeroValue;
	bHasSolidObstacleMask = false;

	UWorld* World = GetWorld();
	if (!World || (!TimeThiefSmokeParameterDefaults::bUseStaticObstacleMask && !TimeThiefSmokeParameterDefaults::bUseBoundsCellCluster))
	{
		++ObstacleMaskRevision;
		ObstacleMaskSnapshot.Reset();
		ObstacleMaskSnapshotRevision = MAX_uint32;
		return;
	}

	const int32 Resolution = TimeThiefSmokeParameterDefaults::ObstacleMaskResolution;
	const int32 CellCount = Resolution * Resolution * Resolution;
	ObstacleMask.Init(0, CellCount);
	ObstacleMaskResolution = Resolution;

	const FVector NaturalBoundsExtent = GetCurrentSmokeBoundsExtent();
	const FVector BoundsExtent = GetCurrentSmokeRenderBoundsExtent();
	const FVector CellHalfExtent = BoundsExtent / static_cast<float>(Resolution);
	const FVector ObstacleQueryExtent = TimeThiefSmokeVolume::MakeObstacleMaskQueryExtent(CellHalfExtent, TimeThiefSmokeParameterDefaults::ObstacleMaskInflation);
	const FTransform SmokeTransform = GetActorTransform();

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TimeThiefSmokeObstacleMask), false);
	QueryParams.AddIgnoredActor(this);
	if (AActor* OwnerActor = GetOwner())
	{
		QueryParams.AddIgnoredActor(OwnerActor);
	}
	if (APawn* InstigatorPawn = GetInstigator())
	{
		QueryParams.AddIgnoredActor(InstigatorPawn);
	}

	TArray<TWeakObjectPtr<UPrimitiveComponent>> StaticObstacleCandidates;
	if (TimeThiefSmokeParameterDefaults::bUseStaticObstacleMask)
	{
		TimeThiefSmokeVolume::QuerySmokeObstacleCandidates(
			World,
			this,
			SmokeTransform,
			BoundsExtent,
			ObstacleQueryExtent,
			QueryParams,
			StaticObstacleCandidates);
	}

	BuildActiveBoundsCells(NaturalBoundsExtent, SmokeTransform, StaticObstacleCandidates);

	for (int32 Z = 0; Z < Resolution; ++Z)
	{
		for (int32 Y = 0; Y < Resolution; ++Y)
		{
			for (int32 X = 0; X < Resolution; ++X)
			{
				const FVector Alpha(
					(static_cast<float>(X) + 0.5f) / static_cast<float>(Resolution),
					(static_cast<float>(Y) + 0.5f) / static_cast<float>(Resolution),
					(static_cast<float>(Z) + 0.5f) / static_cast<float>(Resolution));
				const FVector LocalPosition = ((Alpha * 2.0f) - FVector::OneVector) * BoundsExtent;
				const FVector WorldPosition = SmokeTransform.TransformPosition(LocalPosition);
				const bool bSourceOpen = TimeThiefSmokeVolume::DoesBoxOverlapSourceClearRadius(LocalPosition, CellHalfExtent);
				const FBox CellQueryBounds = TimeThiefSmokeVolume::MakeWorldAabbFromOrientedBox(WorldPosition, SmokeTransform.GetRotation(), ObstacleQueryExtent);
				const bool bMayHitStaticObstacle = TimeThiefSmokeParameterDefaults::bUseStaticObstacleMask &&
					TimeThiefSmokeVolume::AnyCandidateMayOverlap(StaticObstacleCandidates, CellQueryBounds);
				const bool bBlocked = !bSourceOpen &&
					bMayHitStaticObstacle &&
					TimeThiefSmokeVolume::AnyCandidateOverlapsBox(
						StaticObstacleCandidates,
						CellQueryBounds,
						WorldPosition,
						SmokeTransform.GetRotation(),
						FCollisionShape::MakeBox(ObstacleQueryExtent));

				const int32 Index = X + Y * Resolution + Z * Resolution * Resolution;
				if (bBlocked)
				{
					ObstacleMask[Index] = 255;
					bHasSolidObstacleMask = true;
					continue;
				}

				float ActiveOpen = 1.0f;
				if ((LocalPosition.GetAbs() - NaturalBoundsExtent).GetMax() <= 0.0f)
				{
					ActiveOpen = ComputeLocalActiveBoundsOpen(LocalPosition, NaturalBoundsExtent);
				}
				const uint8 MaskValue = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt((1.0f - ActiveOpen) * 255.0f), 0, 255));
				ObstacleMask[Index] = MaskValue;
			}
		}
	}

	++ObstacleMaskRevision;
	ObstacleMaskSnapshot.Reset();
	ObstacleMaskSnapshotRevision = MAX_uint32;
}

void ATimeThiefSmokeVolume::BuildActiveBoundsCells(
	const FVector& BoundsExtent,
	const FTransform& SmokeTransform,
	const TArray<TWeakObjectPtr<UPrimitiveComponent>>& StaticObstacleCandidates)
{
	if (!TimeThiefSmokeParameterDefaults::bUseBoundsCellCluster)
	{
		return;
	}

	ActiveBoundsCellGrid = TimeThiefSmokeParameterDefaults::GetBoundsCellGrid();
	const int32 CellCount = ActiveBoundsCellGrid.X * ActiveBoundsCellGrid.Y * ActiveBoundsCellGrid.Z;
	const int32 MaxActiveCells = TimeThiefSmokeParameterDefaults::MaxActiveBoundsCells;
	TArray<uint8> BlockedCells;
	BlockedCells.Init(0, CellCount);
	ActiveBoundsCells.Init(0, CellCount);
	TArray<FIntVector> SourceSeedCells;
	SourceSeedCells.Reserve(CellCount);

	const FVector CellHalfExtent(
		BoundsExtent.X / static_cast<float>(ActiveBoundsCellGrid.X),
		BoundsExtent.Y / static_cast<float>(ActiveBoundsCellGrid.Y),
		BoundsExtent.Z / static_cast<float>(ActiveBoundsCellGrid.Z));
	const FVector ObstacleQueryExtent = TimeThiefSmokeVolume::MakeBoundsCellObstacleQueryExtent(CellHalfExtent);

	for (int32 Z = 0; Z < ActiveBoundsCellGrid.Z; ++Z)
	{
		for (int32 Y = 0; Y < ActiveBoundsCellGrid.Y; ++Y)
		{
			for (int32 X = 0; X < ActiveBoundsCellGrid.X; ++X)
			{
				const FIntVector Coord(X, Y, Z);
				const FVector LocalCenter = TimeThiefSmokeVolume::CellCoordToLocalCenter(Coord, BoundsExtent, BoundsClusterLocalOffset, ActiveBoundsCellGrid);
				const FVector WorldCenter = SmokeTransform.TransformPosition(LocalCenter);
				const bool bSourceCell = TimeThiefSmokeVolume::DoesCellOverlapSourceClearRadius(Coord, BoundsExtent, BoundsClusterLocalOffset, ActiveBoundsCellGrid);
				const bool bOutsideBounds = (LocalCenter.GetAbs() - BoundsExtent).GetMax() > 0.0f;
				const FBox CellQueryBounds = TimeThiefSmokeVolume::MakeWorldAabbFromOrientedBox(WorldCenter, SmokeTransform.GetRotation(), ObstacleQueryExtent);
				const bool bMayHitStaticObstacle = TimeThiefSmokeParameterDefaults::bUseStaticObstacleMask &&
					TimeThiefSmokeVolume::AnyCandidateMayOverlap(StaticObstacleCandidates, CellQueryBounds);
				const bool bBlocked = bOutsideBounds ||
					(!bSourceCell &&
						bMayHitStaticObstacle &&
						TimeThiefSmokeVolume::AnyCandidateOverlapsBox(
							StaticObstacleCandidates,
							CellQueryBounds,
							WorldCenter,
							SmokeTransform.GetRotation(),
							FCollisionShape::MakeBox(ObstacleQueryExtent)));

				if (bSourceCell)
				{
					SourceSeedCells.Add(Coord);
				}

				if (bBlocked)
				{
					BlockedCells[TimeThiefSmokeVolume::FlattenCellCoord(Coord, ActiveBoundsCellGrid)] = 1;
				}
			}
		}
	}

	TArray<FIntVector> Queue;
	Queue.Reserve(CellCount);
	for (const FIntVector& SourceCoord : SourceSeedCells)
	{
		const int32 SourceCellIndex = TimeThiefSmokeVolume::FlattenCellCoord(SourceCoord, ActiveBoundsCellGrid);
		if (BlockedCells[SourceCellIndex] == 0)
		{
			Queue.Add(SourceCoord);
		}
	}

	FIntVector StartCoord(
		ActiveBoundsCellGrid.X / 2,
		ActiveBoundsCellGrid.Y / 2,
		ActiveBoundsCellGrid.Z / 2);
	if (Queue.IsEmpty() &&
		(!TimeThiefSmokeVolume::IsCellCoordValid(StartCoord, ActiveBoundsCellGrid) ||
			BlockedCells[TimeThiefSmokeVolume::FlattenCellCoord(StartCoord, ActiveBoundsCellGrid)] != 0))
	{
		float BestDistanceSq = TNumericLimits<float>::Max();
		for (int32 CellIndex = 0; CellIndex < CellCount; ++CellIndex)
		{
			if (BlockedCells[CellIndex] != 0)
			{
				continue;
			}

			const FIntVector Coord(
				CellIndex % ActiveBoundsCellGrid.X,
				(CellIndex / ActiveBoundsCellGrid.X) % ActiveBoundsCellGrid.Y,
				CellIndex / (ActiveBoundsCellGrid.X * ActiveBoundsCellGrid.Y));
			const FVector CellDelta(
				static_cast<float>(Coord.X - ActiveBoundsCellGrid.X / 2),
				static_cast<float>(Coord.Y - ActiveBoundsCellGrid.Y / 2),
				static_cast<float>(Coord.Z - ActiveBoundsCellGrid.Z / 2));
			const float DistanceSq = CellDelta.SizeSquared();
			if (DistanceSq < BestDistanceSq)
			{
				BestDistanceSq = DistanceSq;
				StartCoord = Coord;
			}
		}
	}

	if (Queue.IsEmpty() && !TimeThiefSmokeVolume::IsCellCoordValid(StartCoord, ActiveBoundsCellGrid))
	{
		return;
	}

	if (Queue.IsEmpty())
	{
		Queue.Add(StartCoord);
	}
	int32 ReadIndex = 0;
	int32 ActiveCount = 0;

	static const FIntVector Neighbors[] =
	{
		FIntVector(1, 0, 0),
		FIntVector(-1, 0, 0),
		FIntVector(0, 1, 0),
		FIntVector(0, -1, 0),
		FIntVector(0, 0, 1),
		FIntVector(0, 0, -1)
	};

	while (ReadIndex < Queue.Num() && ActiveCount < MaxActiveCells)
	{
		const FIntVector Coord = Queue[ReadIndex++];
		if (!TimeThiefSmokeVolume::IsCellCoordValid(Coord, ActiveBoundsCellGrid))
		{
			continue;
		}

		const int32 CellIndex = TimeThiefSmokeVolume::FlattenCellCoord(Coord, ActiveBoundsCellGrid);
		if (BlockedCells[CellIndex] != 0 || ActiveBoundsCells[CellIndex] != 0)
		{
			continue;
		}

		ActiveBoundsCells[CellIndex] = 1;
		++ActiveCount;

		for (const FIntVector& Neighbor : Neighbors)
		{
			const FIntVector NextCoord = Coord + Neighbor;
			if (TimeThiefSmokeVolume::IsCellCoordValid(NextCoord, ActiveBoundsCellGrid))
			{
				Queue.Add(NextCoord);
			}
		}
	}
}

float ATimeThiefSmokeVolume::ComputeLocalActiveBoundsOpen(const FVector& LocalPosition, const FVector& BoundsExtent) const
{
	if (!TimeThiefSmokeParameterDefaults::bUseBoundsCellCluster || ActiveBoundsCells.IsEmpty())
	{
		return 1.0f;
	}

	if (LocalPosition.SizeSquared() <= FMath::Square(TimeThiefSmokeVolume::GetSourceClearRadius()))
	{
		return 1.0f;
	}

	const FVector Relative = LocalPosition - BoundsClusterLocalOffset;
	const FVector Alpha = (Relative / BoundsExtent) * 0.5f + FVector(0.5f);
	const FVector CellPosition(
		Alpha.X * static_cast<float>(ActiveBoundsCellGrid.X),
		Alpha.Y * static_cast<float>(ActiveBoundsCellGrid.Y),
		Alpha.Z * static_cast<float>(ActiveBoundsCellGrid.Z));

	float NearestDistanceSq = TNumericLimits<float>::Max();
	for (int32 CellIndex = 0; CellIndex < ActiveBoundsCells.Num(); ++CellIndex)
	{
		if (ActiveBoundsCells[CellIndex] == 0)
		{
			continue;
		}

		const FIntVector Coord(
			CellIndex % ActiveBoundsCellGrid.X,
			(CellIndex / ActiveBoundsCellGrid.X) % ActiveBoundsCellGrid.Y,
			CellIndex / (ActiveBoundsCellGrid.X * ActiveBoundsCellGrid.Y));
		const FVector CellMin(static_cast<float>(Coord.X), static_cast<float>(Coord.Y), static_cast<float>(Coord.Z));
		const FVector CellMax = CellMin + FVector::OneVector;
		const FVector Delta(
			FMath::Max(FMath::Max(CellMin.X - CellPosition.X, 0.0f), CellPosition.X - CellMax.X),
			FMath::Max(FMath::Max(CellMin.Y - CellPosition.Y, 0.0f), CellPosition.Y - CellMax.Y),
			FMath::Max(FMath::Max(CellMin.Z - CellPosition.Z, 0.0f), CellPosition.Z - CellMax.Z));
		NearestDistanceSq = FMath::Min(NearestDistanceSq, Delta.SizeSquared());
		if (NearestDistanceSq <= KINDA_SMALL_NUMBER)
		{
			return 1.0f;
		}
	}

	if (NearestDistanceSq == TNumericLimits<float>::Max())
	{
		return 1.0f;
	}

	const float FeatherCells = TimeThiefSmokeParameterDefaults::ActiveBoundsOpenFeatherCells;
	return 1.0f - TimeThiefSmokeVolume::SmoothStep01(FMath::Sqrt(NearestDistanceSq) / FeatherCells);
}

void ATimeThiefSmokeVolume::UpdateSmokeBounds()
{
	if (SmokeBoundsComponent)
	{
		SmokeBoundsComponent->SetBoxExtent(GetCurrentSmokeBoundsExtent(), true);
	}
}

void ATimeThiefSmokeVolume::DrawDebugSmoke() const
{
	if (!GetWorld() || !SmokeBoundsComponent)
	{
		return;
	}

	DrawDebugBox(
		GetWorld(),
		SmokeBoundsComponent->GetComponentLocation(),
		SmokeBoundsComponent->GetScaledBoxExtent(),
		SmokeBoundsComponent->GetComponentQuat(),
		FColor::Cyan,
		false,
		0.0f,
		0,
		1.5f);

	DrawDebugBox(
		GetWorld(),
		SmokeBoundsComponent->GetComponentLocation(),
		GetCurrentSmokeRenderBoundsExtent() * SmokeBoundsComponent->GetComponentScale(),
		SmokeBoundsComponent->GetComponentQuat(),
		FColor(180, 80, 255),
		false,
		0.0f,
		0,
		1.0f);

	if (TimeThiefSmokeParameterDefaults::bUseBoundsCellCluster && !ActiveBoundsCells.IsEmpty())
	{
		const FVector BoundsExtent = GetCurrentSmokeBoundsExtent();
		const FVector CellExtent(
			BoundsExtent.X / static_cast<float>(ActiveBoundsCellGrid.X),
			BoundsExtent.Y / static_cast<float>(ActiveBoundsCellGrid.Y),
			BoundsExtent.Z / static_cast<float>(ActiveBoundsCellGrid.Z));
		const FTransform SmokeTransform = GetActorTransform();

		for (int32 CellIndex = 0; CellIndex < ActiveBoundsCells.Num(); ++CellIndex)
		{
			if (ActiveBoundsCells[CellIndex] == 0)
			{
				continue;
			}

			const FIntVector Coord(
				CellIndex % ActiveBoundsCellGrid.X,
				(CellIndex / ActiveBoundsCellGrid.X) % ActiveBoundsCellGrid.Y,
				CellIndex / (ActiveBoundsCellGrid.X * ActiveBoundsCellGrid.Y));
			const FVector LocalCenter = TimeThiefSmokeVolume::CellCoordToLocalCenter(Coord, BoundsExtent, BoundsClusterLocalOffset, ActiveBoundsCellGrid);
			DrawDebugBox(
				GetWorld(),
				SmokeTransform.TransformPosition(LocalCenter),
				CellExtent * 0.48f,
				SmokeTransform.GetRotation(),
				FColor::Green,
				false,
				0.0f,
				0,
				0.75f);
		}
	}
}
