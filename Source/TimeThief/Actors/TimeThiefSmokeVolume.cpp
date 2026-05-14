#include "Actors/TimeThiefSmokeVolume.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SphereComponent.h"
#include "Components/ShapeComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Smoke/TimeThiefSmokeWorldSubsystem.h"
#include "WorldCollision.h"

namespace TimeThiefSmokeVolume
{
	int32 GNextSmokeId = 1;

	FIntVector ClampCellGrid(const FIntVector& CellGrid)
	{
		return FIntVector(
			FMath::Clamp(CellGrid.X, 1, 16),
			FMath::Clamp(CellGrid.Y, 1, 16),
			FMath::Clamp(CellGrid.Z, 1, 12));
	}

	bool IsCellCoordValid(const FIntVector& Coord, const FIntVector& CellGrid)
	{
		return Coord.X >= 0 && Coord.Y >= 0 && Coord.Z >= 0 &&
			Coord.X < CellGrid.X && Coord.Y < CellGrid.Y && Coord.Z < CellGrid.Z;
	}

	int32 FlattenCellCoord(const FIntVector& Coord, const FIntVector& CellGrid)
	{
		return Coord.X + Coord.Y * CellGrid.X + Coord.Z * CellGrid.X * CellGrid.Y;
	}

	FIntVector LocalToCellCoord(const FVector& LocalPosition, const FVector& BoundsExtent, const FVector& ClusterOffset, const FIntVector& CellGrid)
	{
		const FVector Relative = LocalPosition - ClusterOffset;
		const FVector Alpha = (Relative / BoundsExtent.ComponentMax(FVector(1.0f))) * 0.5f + FVector(0.5f);
		return FIntVector(
			FMath::FloorToInt(Alpha.X * static_cast<float>(CellGrid.X)),
			FMath::FloorToInt(Alpha.Y * static_cast<float>(CellGrid.Y)),
			FMath::FloorToInt(Alpha.Z * static_cast<float>(CellGrid.Z)));
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
		const FVector MaxOffset = BoundsExtent * 0.42f;
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

}

ATimeThiefSmokeVolume::ATimeThiefSmokeVolume()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	SmokeBoundsComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("SmokeBounds"));
	SetRootComponent(SmokeBoundsComponent);
	SmokeBoundsComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SmokeBoundsComponent->SetCollisionObjectType(ECC_WorldDynamic);
	SmokeBoundsComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	SmokeBoundsComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	SmokeBoundsComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	SmokeBoundsComponent->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	SmokeBoundsComponent->SetGenerateOverlapEvents(false);
}

void ATimeThiefSmokeVolume::InitializeSmokeVolume(const FTimeThiefSmokeRuntimeSettings& InSettings, AActor* InOwnerActor, APawn* InInstigatorPawn)
{
	SmokeSettings = InSettings;
	SmokeId = TimeThiefSmokeVolume::GNextSmokeId++;
	SmokeAgeSeconds = 0.0f;
	DynamicBoundsExtent = FVector::ZeroVector;
	BoundsClusterLocalOffset = FVector::ZeroVector;
	ActiveBoundsCells.Reset();
	ActiveBoundsCellGrid = FIntVector::ZeroValue;

	SetOwner(InOwnerActor);
	SetInstigator(InInstigatorPawn);
	SetLifeSpan(FMath::Max(0.1f, SmokeSettings.SmokeDuration + SmokeSettings.SmokeFadeOutDuration));

	UpdateSmokeBounds();
	RebuildStaticObstacleMask();

	ControlGrid.Initialize(
		GetWorld(),
		GetActorTransform(),
		SmokeSettings.SmokeBoundsExtent,
		SmokeSettings.SmokeControlGridResolution,
		SmokeSettings.InitialDensity,
		this);
}

FVector ATimeThiefSmokeVolume::GetCurrentSmokeBoundsExtent() const
{
	const FVector BaseExtent = SmokeSettings.SmokeBoundsExtent.ComponentMax(FVector(1.0f));
	const float FlowExpansion = FMath::Min(FMath::Max(0.0f, SmokeSettings.SmokeBoundsExpansionSpeed) * FMath::Max(0.0f, SmokeSettings.SmokeDuration) * 0.35f, 420.0f);
	const float ShockExpansion = FMath::Max(0.0f, SmokeSettings.ExplosionOutwardStrength) * FMath::Max(0.0f, SmokeSettings.ExplosionImpulseDuration) * 0.35f;
	const float ReservedExpansion = FMath::Max(FlowExpansion, ShockExpansion);
	const FVector ReservedExtent = BaseExtent + FVector(ReservedExpansion, ReservedExpansion, ReservedExpansion * 0.75f);
	return ReservedExtent.ComponentMax(DynamicBoundsExtent);
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
	const FVector Extent = SmokeBoundsComponent->GetUnscaledBoxExtent();

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

	const FVector ClosestPoint = SmokeBoundsComponent->Bounds.GetBox().GetClosestPointTo(Center);
	return FVector::DistSquared(ClosestPoint, Center) <= FMath::Square(FMath::Max(1.0f, Radius));
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
	Event.Direction = Direction;
	Event.Rotation = Direction.Rotation().Quaternion();
	const float VisibleClearRadius = FMath::Max(SmokeSettings.BulletClearRadius, 42.0f);
	Event.Radius = VisibleClearRadius * RandomStream.FRandRange(0.95f, 1.2f);
	Event.Length = SegmentLength + Event.Radius * 2.0f;
	Event.Strength = FMath::Clamp(Strength * RandomStream.FRandRange(0.92f, 1.0f), 0.0f, 1.0f);
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
	Event.Direction = (GetActorLocation() - Center).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	Event.Rotation = FQuat::Identity;
	Event.Radius = FMath::Max(Radius, SmokeSettings.ExplosionShockRadius);
	Event.Length = Event.Radius;
	Event.Strength = FMath::Max(0.0f, Strength);
	Event.Extents = FVector(SmokeSettings.ExplosionOutwardStrength, SmokeSettings.ExplosionDensityClearStrength, 0.0f);
	Event.NormalizedAge = 0.0f;
	Event.Seed = Seed;

	if (UTimeThiefSmokeWorldSubsystem* SmokeSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UTimeThiefSmokeWorldSubsystem>() : nullptr)
	{
		SmokeSubsystem->AddTimedInteractionEvent(this, Event, SmokeSettings.ExplosionImpulseDuration);
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

		const float ShockTravel = FMath::Max(0.0f, Event.Extents.X) * FMath::Max(0.0f, SmokeSettings.ExplosionImpulseDuration) * 0.45f;
		ExpandDynamicBoundsForWorldSphere(Event.Position, Event.Radius + ShockTravel);
	}

	ControlGrid.ApplyInteractionEvent(Event);

	if (UTimeThiefSmokeWorldSubsystem* SmokeSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UTimeThiefSmokeWorldSubsystem>() : nullptr)
	{
		SmokeSubsystem->RecordRendererEvent(Event);
	}
}

void ATimeThiefSmokeVolume::BeginPlay()
{
	Super::BeginPlay();

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
	ControlGrid.Tick(DeltaTime);
	GatherActorPushEvents(DeltaTime);

	if (SmokeSettings.bDrawDebugBounds)
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

	const float SafeHz = FMath::Max(1.0f, SmokeSettings.ActorInteractionHz);
	ActorInteractionAccumulator += DeltaTime;
	if (ActorInteractionAccumulator < (1.0f / SafeHz))
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

	TArray<FOverlapResult> Overlaps;
	const bool bAnyOverlap = GetWorld()->OverlapMultiByObjectType(
		Overlaps,
		SmokeBoundsComponent->GetComponentLocation(),
		SmokeBoundsComponent->GetComponentQuat(),
		ObjectQueryParams,
		FCollisionShape::MakeBox(SmokeBoundsComponent->GetScaledBoxExtent()),
		QueryParams);

	if (!bAnyOverlap)
	{
		PreviousComponentLocations.Reset();
		return;
	}

	int32 EventsSubmitted = 0;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (EventsSubmitted >= SmokeSettings.MaxActorInteractionEventsPerTick)
		{
			break;
		}

		UPrimitiveComponent* PrimitiveComponent = Overlap.GetComponent();
		if (!PrimitiveComponent || PrimitiveComponent == SmokeBoundsComponent || PrimitiveComponent->Mobility == EComponentMobility::Static)
		{
			continue;
		}

		FTimeThiefSmokeInteractionEvent Event;
		MakeActorPushEvent(PrimitiveComponent, SampleDeltaTime, Event);
		if (Event.Strength <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		ApplyInteractionEvent(Event);
		++EventsSubmitted;
	}
}

void ATimeThiefSmokeVolume::MakeActorPushEvent(UPrimitiveComponent* PrimitiveComponent, float DeltaTime, FTimeThiefSmokeInteractionEvent& OutEvent) const
{
	if (!PrimitiveComponent)
	{
		return;
	}

	const FVector Velocity = const_cast<ATimeThiefSmokeVolume*>(this)->ResolveComponentVelocity(PrimitiveComponent, DeltaTime);
	const float Speed = Velocity.Size();
	if (Speed < SmokeSettings.ActorPushVelocityThreshold)
	{
		return;
	}

	OutEvent.SmokeId = SmokeId;
	OutEvent.Type = ESmokeInteractionType::ActorPush;
	OutEvent.Position = PrimitiveComponent->Bounds.Origin;
	OutEvent.Direction = Velocity.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
	OutEvent.Rotation = PrimitiveComponent->GetComponentQuat();
	OutEvent.Strength = FMath::Clamp(Speed / 600.0f, 0.15f, 1.0f);
	OutEvent.NormalizedAge = 0.0f;
	OutEvent.Seed = GetTypeHash(PrimitiveComponent);
	OutEvent.Shape = ResolvePrimitiveShape(PrimitiveComponent, OutEvent);
}

FVector ATimeThiefSmokeVolume::ResolveComponentVelocity(UPrimitiveComponent* PrimitiveComponent, float DeltaTime)
{
	if (!PrimitiveComponent)
	{
		return FVector::ZeroVector;
	}

	FVector Velocity = PrimitiveComponent->GetComponentVelocity();
	const FVector CurrentLocation = PrimitiveComponent->GetComponentLocation();

	if (Velocity.IsNearlyZero() && DeltaTime > KINDA_SMALL_NUMBER)
	{
		if (const FVector* PreviousLocation = PreviousComponentLocations.Find(PrimitiveComponent))
		{
			Velocity = (CurrentLocation - *PreviousLocation) / DeltaTime;
		}
	}

	PreviousComponentLocations.FindOrAdd(PrimitiveComponent) = CurrentLocation;
	return Velocity;
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
		OutEvent.Radius = OutEvent.Extents.GetMax() * 0.35f;
		OutEvent.Length = OutEvent.Extents.GetMax();
		return ESmokeInteractionShape::Box;
	}

	OutEvent.Extents = PrimitiveComponent->Bounds.BoxExtent;
	OutEvent.Radius = OutEvent.Extents.GetMax() * 0.35f;
	OutEvent.Length = OutEvent.Extents.GetMax();
	return ESmokeInteractionShape::Box;
}

void ATimeThiefSmokeVolume::ExpandDynamicBoundsForWorldSphere(const FVector& Center, float Radius)
{
	const FVector LocalCenter = GetActorTransform().InverseTransformPosition(Center).GetAbs();
	const FVector RequiredExtent = LocalCenter + FVector(FMath::Max(1.0f, Radius));
	const FVector PreviousExtent = DynamicBoundsExtent;
	DynamicBoundsExtent = DynamicBoundsExtent.ComponentMax(RequiredExtent);
	if (!DynamicBoundsExtent.Equals(PreviousExtent, 1.0f))
	{
		UpdateSmokeBounds();
		RebuildStaticObstacleMask();
	}
}

void ATimeThiefSmokeVolume::ShiftBoundsClusterForExplosion(const FTimeThiefSmokeInteractionEvent& Event)
{
	if (!SmokeSettings.bUseBoundsCellCluster)
	{
		return;
	}

	const FVector WorldDirection = (GetActorLocation() - Event.Position).GetSafeNormal(UE_SMALL_NUMBER, Event.Direction.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector));
	const float ShiftDistance = FMath::Max(0.0f, Event.Extents.X) * FMath::Max(0.0f, SmokeSettings.ExplosionImpulseDuration) * FMath::Max(0.0f, SmokeSettings.ExplosionBoundsShiftScale) * FMath::Max(0.0f, Event.Strength);
	const FVector LocalShift = GetActorTransform().InverseTransformVectorNoScale(WorldDirection * ShiftDistance);
	const FVector PreviousOffset = BoundsClusterLocalOffset;
	BoundsClusterLocalOffset = TimeThiefSmokeVolume::ClampClusterOffset(BoundsClusterLocalOffset + LocalShift, GetCurrentSmokeBoundsExtent().ComponentMax(FVector(1.0f)));
	if (!BoundsClusterLocalOffset.Equals(PreviousOffset, 1.0f))
	{
		RebuildStaticObstacleMask();
	}
}

void ATimeThiefSmokeVolume::RebuildStaticObstacleMask()
{
	ObstacleMask.Reset();
	ObstacleMaskResolution = 0;
	ActiveBoundsCells.Reset();
	ActiveBoundsCellGrid = FIntVector::ZeroValue;

	UWorld* World = GetWorld();
	if (!World || !SmokeSettings.bUseStaticObstacleMask)
	{
		++ObstacleMaskRevision;
		return;
	}

	const int32 Resolution = FMath::Clamp(SmokeSettings.ObstacleMaskResolution, 8, 64);
	const int32 CellCount = Resolution * Resolution * Resolution;
	ObstacleMask.Init(0, CellCount);
	ObstacleMaskResolution = Resolution;

	const FVector BoundsExtent = GetCurrentSmokeBoundsExtent().ComponentMax(FVector(1.0f));
	const FVector CellExtent = (BoundsExtent / static_cast<float>(Resolution)) + FVector(FMath::Max(0.0f, SmokeSettings.ObstacleMaskInflation));
	const FTransform SmokeTransform = GetActorTransform();

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TimeThiefSmokeObstacleMask), false);
	QueryParams.AddIgnoredActor(this);
	BuildActiveBoundsCells(BoundsExtent, SmokeTransform, ObjectQueryParams, QueryParams);

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
				const bool bBlocked = World->OverlapAnyTestByObjectType(
					WorldPosition,
					SmokeTransform.GetRotation(),
					ObjectQueryParams,
					FCollisionShape::MakeBox(CellExtent),
					QueryParams);

				const int32 Index = X + Y * Resolution + Z * Resolution * Resolution;
				if (bBlocked)
				{
					ObstacleMask[Index] = 255;
					continue;
				}

				const float ActiveOpen = ComputeLocalActiveBoundsOpen(LocalPosition, BoundsExtent);
				ObstacleMask[Index] = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt((1.0f - ActiveOpen) * 255.0f), 0, 255));
			}
		}
	}

	++ObstacleMaskRevision;
}

void ATimeThiefSmokeVolume::BuildActiveBoundsCells(
	const FVector& BoundsExtent,
	const FTransform& SmokeTransform,
	FCollisionObjectQueryParams ObjectQueryParams,
	FCollisionQueryParams QueryParams)
{
	if (!SmokeSettings.bUseBoundsCellCluster)
	{
		return;
	}

	ActiveBoundsCellGrid = TimeThiefSmokeVolume::ClampCellGrid(SmokeSettings.BoundsCellGrid);
	const int32 CellCount = ActiveBoundsCellGrid.X * ActiveBoundsCellGrid.Y * ActiveBoundsCellGrid.Z;
	const int32 MaxActiveCells = FMath::Clamp(SmokeSettings.MaxActiveBoundsCells, 1, CellCount);
	TArray<uint8> BlockedCells;
	BlockedCells.Init(0, CellCount);
	ActiveBoundsCells.Init(0, CellCount);

	const FVector CellExtent(
		BoundsExtent.X / static_cast<float>(ActiveBoundsCellGrid.X),
		BoundsExtent.Y / static_cast<float>(ActiveBoundsCellGrid.Y),
		BoundsExtent.Z / static_cast<float>(ActiveBoundsCellGrid.Z));

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (int32 Z = 0; Z < ActiveBoundsCellGrid.Z; ++Z)
	{
		for (int32 Y = 0; Y < ActiveBoundsCellGrid.Y; ++Y)
		{
			for (int32 X = 0; X < ActiveBoundsCellGrid.X; ++X)
			{
				const FIntVector Coord(X, Y, Z);
				const FVector LocalCenter = TimeThiefSmokeVolume::CellCoordToLocalCenter(Coord, BoundsExtent, BoundsClusterLocalOffset, ActiveBoundsCellGrid);
				const bool bOutsideBounds = (LocalCenter.GetAbs() - BoundsExtent).GetMax() > 0.0f;
				const bool bBlocked = bOutsideBounds || World->OverlapAnyTestByObjectType(
					SmokeTransform.TransformPosition(LocalCenter),
					SmokeTransform.GetRotation(),
					ObjectQueryParams,
					FCollisionShape::MakeBox(CellExtent * 0.48f),
					QueryParams);

				if (bBlocked)
				{
					BlockedCells[TimeThiefSmokeVolume::FlattenCellCoord(Coord, ActiveBoundsCellGrid)] = 1;
				}
			}
		}
	}

	FIntVector StartCoord(
		ActiveBoundsCellGrid.X / 2,
		ActiveBoundsCellGrid.Y / 2,
		ActiveBoundsCellGrid.Z / 2);
	if (!TimeThiefSmokeVolume::IsCellCoordValid(StartCoord, ActiveBoundsCellGrid) ||
		BlockedCells[TimeThiefSmokeVolume::FlattenCellCoord(StartCoord, ActiveBoundsCellGrid)] != 0)
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

	if (!TimeThiefSmokeVolume::IsCellCoordValid(StartCoord, ActiveBoundsCellGrid))
	{
		return;
	}

	TArray<FIntVector> Queue;
	Queue.Reserve(CellCount);
	Queue.Add(StartCoord);
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
	if (!SmokeSettings.bUseBoundsCellCluster || ActiveBoundsCells.IsEmpty())
	{
		return 1.0f;
	}

	if (LocalPosition.SizeSquared() <= FMath::Square(FMath::Max(1.0f, SmokeSettings.PlumeSourceRadius) * 1.25f))
	{
		return 1.0f;
	}

	const FVector Relative = LocalPosition - BoundsClusterLocalOffset;
	const FVector Alpha = (Relative / BoundsExtent.ComponentMax(FVector(1.0f))) * 0.5f + FVector(0.5f);
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

	const float FeatherCells = 1.35f;
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

	if (SmokeSettings.bUseBoundsCellCluster && !ActiveBoundsCells.IsEmpty())
	{
		const FVector BoundsExtent = GetCurrentSmokeBoundsExtent().ComponentMax(FVector(1.0f));
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
