#include "TimeThiefSmokeTestMover.h"
#include "TimeThiefSmokeTestObstacle.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"

namespace
{
	void DisableShape(UShapeComponent* Shape)
	{
		Shape->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Shape->SetGenerateOverlapEvents(false);
		Shape->SetVisibility(false);
	}

	void EnableMoverShape(UShapeComponent* Shape)
	{
		Shape->SetMobility(EComponentMobility::Movable);
		Shape->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Shape->SetCollisionObjectType(ECC_WorldDynamic);
		Shape->SetCollisionResponseToAllChannels(ECR_Overlap);
		Shape->SetGenerateOverlapEvents(true);
		Shape->SetVisibility(true);
	}

	void EnableObstacleShape(UShapeComponent* Shape)
	{
		Shape->SetMobility(EComponentMobility::Static);
		Shape->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Shape->SetCollisionObjectType(ECC_WorldStatic);
		Shape->SetCollisionResponseToAllChannels(ECR_Ignore);
		Shape->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		Shape->SetGenerateOverlapEvents(false);
		Shape->SetVisibility(true);
	}
}

ATimeThiefSmokeTestMover::ATimeThiefSmokeTestMover()
{
	PrimaryActorTick.bCanEverTick = true;
	BoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	SphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	SetRootComponent(BoxComponent);
	CapsuleComponent->SetupAttachment(BoxComponent);
	SphereComponent->SetupAttachment(BoxComponent);
	DisableShape(BoxComponent);
	DisableShape(CapsuleComponent);
	DisableShape(SphereComponent);
}

void ATimeThiefSmokeTestMover::Configure(const FTimeThiefSmokeTestMoverSettings& Settings)
{
	DisableShape(BoxComponent);
	DisableShape(CapsuleComponent);
	DisableShape(SphereComponent);
	StartPosition = Settings.Start;
	EndPosition = Settings.End;
	Duration = FMath::Max(Settings.Duration, KINDA_SMALL_NUMBER);
	StartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	SetActorLocation(StartPosition);

	if (Settings.Shape == TEXT("sphere"))
	{
		SphereComponent->SetSphereRadius(FMath::Max(1.0f, Settings.Radius));
		EnableMoverShape(SphereComponent);
	}
	else if (Settings.Shape == TEXT("capsule"))
	{
		CapsuleComponent->SetCapsuleSize(FMath::Max(1.0f, Settings.Radius), FMath::Max(Settings.Radius, Settings.HalfHeight));
		EnableMoverShape(CapsuleComponent);
	}
	else
	{
		BoxComponent->SetBoxExtent(Settings.Extent.ComponentMax(FVector(1.0)));
		EnableMoverShape(BoxComponent);
	}
}

void ATimeThiefSmokeTestMover::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : StartTime;
	const float Alpha = FMath::Clamp((Now - StartTime) / Duration, 0.0f, 1.0f);
	SetActorLocation(FMath::Lerp(StartPosition, EndPosition, Alpha), false, nullptr, ETeleportType::TeleportPhysics);
}

ATimeThiefSmokeTestObstacle::ATimeThiefSmokeTestObstacle()
{
	PrimaryActorTick.bCanEverTick = false;
	BoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	SphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	SetRootComponent(BoxComponent);
	CapsuleComponent->SetupAttachment(BoxComponent);
	SphereComponent->SetupAttachment(BoxComponent);
	BoxComponent->SetMobility(EComponentMobility::Static);
	CapsuleComponent->SetMobility(EComponentMobility::Static);
	SphereComponent->SetMobility(EComponentMobility::Static);
	DisableShape(BoxComponent);
	DisableShape(CapsuleComponent);
	DisableShape(SphereComponent);
}

void ATimeThiefSmokeTestObstacle::Configure(const FTimeThiefSmokeTestObstacleSettings& Settings)
{
	DisableShape(BoxComponent);
	DisableShape(CapsuleComponent);
	DisableShape(SphereComponent);

	if (Settings.Shape == TEXT("sphere"))
	{
		SphereComponent->SetSphereRadius(FMath::Max(1.0f, Settings.Radius));
		EnableObstacleShape(SphereComponent);
	}
	else if (Settings.Shape == TEXT("capsule"))
	{
		CapsuleComponent->SetCapsuleSize(FMath::Max(1.0f, Settings.Radius), FMath::Max(Settings.Radius, Settings.HalfHeight));
		EnableObstacleShape(CapsuleComponent);
	}
	else
	{
		BoxComponent->SetBoxExtent(Settings.Extent.ComponentMax(FVector(1.0)));
		EnableObstacleShape(BoxComponent);
	}
}
