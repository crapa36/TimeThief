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

	bool IntersectSegmentBox(const FVector& SegmentStart, const FVector& SegmentDelta, const FVector& BoxCenter, const FVector& BoxExtent, float& OutTMin, float& OutTMax)
	{
		float TMin = 0.0f;
		float TMax = 1.0f;

		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			const float StartValue = SegmentStart[Axis];
			const float DeltaValue = SegmentDelta[Axis];
			const float MinValue = BoxCenter[Axis] - BoxExtent[Axis];
			const float MaxValue = BoxCenter[Axis] + BoxExtent[Axis];

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

		OutTMin = TMin;
		OutTMax = TMax;
		return true;
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
	return SmokeSettings.SmokeBoundsExtent.ComponentMax(FVector(1.0f));
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

	float TMin = 0.0f;
	float TMax = 1.0f;
	const FVector Extent = SmokeBoundsComponent->GetUnscaledBoxExtent();
	if (!TimeThiefSmokeVolume::IntersectSegmentBox(LocalStart, LocalDelta, FVector::ZeroVector, Extent, TMin, TMax))
	{
		return false;
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
	OutEvent.Strength = FMath::Clamp(Speed / 600.0f, 0.0f, 1.0f);
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

void ATimeThiefSmokeVolume::RebuildStaticObstacleMask()
{
	ObstacleMask.Reset();
	ObstacleMaskResolution = 0;

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
				ObstacleMask[Index] = bBlocked ? 255 : 0;
			}
		}
	}

	++ObstacleMaskRevision;
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
}
