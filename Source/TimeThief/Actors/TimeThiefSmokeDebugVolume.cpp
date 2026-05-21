#include "Actors/TimeThiefSmokeDebugVolume.h"

#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "TimeThiefSmokeParameterDefaults.h"

ATimeThiefSmokeDebugVolume::ATimeThiefSmokeDebugVolume()
	: Radius(TimeThiefSmokeParameterDefaults::SmokeDebugRadius)
	, Duration(TimeThiefSmokeParameterDefaults::SmokeDebugDuration)
	, DebugSegments(TimeThiefSmokeParameterDefaults::SmokeDebugSegments)
	, DebugColor(TimeThiefSmokeParameterDefaults::GetSmokeDebugColor())
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void ATimeThiefSmokeDebugVolume::InitializeSmokeDebug(float InRadius, float InDuration)
{
	Radius = FMath::Max(1.0f, InRadius);
	Duration = FMath::Max(0.1f, InDuration);
	SetLifeSpan(Duration);

#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log, TEXT("[Throwable][Smoke] Debug volume spawned. Radius=%.1f Duration=%.1f Location=%s"),
		Radius,
		Duration,
		*GetActorLocation().ToCompactString());
#endif
}

void ATimeThiefSmokeDebugVolume::BeginPlay()
{
	Super::BeginPlay();
	SetLifeSpan(Duration);
}

void ATimeThiefSmokeDebugVolume::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (UWorld* World = GetWorld())
	{
		DrawDebugSphere(World, GetActorLocation(), Radius, DebugSegments, DebugColor, false, 0.0f, 0, 1.5f);
	}
}
