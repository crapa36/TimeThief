#pragma once

#include "CoreMinimal.h"
#include "TimeThiefSmokeTypes.generated.h"

class ATimeThiefSmokeVolume;

UENUM(BlueprintType)
enum class ESmokeInteractionType : uint8
{
	BulletWake,
	ExplosionShock,
	ActorPush
};

UENUM(BlueprintType)
enum class ESmokeInteractionShape : uint8
{
	Sphere,
	Capsule,
	Box,
	LineWake
};

USTRUCT(BlueprintType)
struct FTimeThiefSmokeInteractionEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|Smoke")
	int32 SmokeId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|Smoke")
	ESmokeInteractionType Type = ESmokeInteractionType::BulletWake;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|Smoke")
	ESmokeInteractionShape Shape = ESmokeInteractionShape::Sphere;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|Smoke")
	FVector Position = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|Smoke")
	FVector Direction = FVector::ForwardVector;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|Smoke")
	FQuat Rotation = FQuat::Identity;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|Smoke")
	FVector Extents = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|Smoke")
	float Radius = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|Smoke")
	float Length = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|Smoke")
	float Strength = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|Smoke")
	float NormalizedAge = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|Smoke")
	int32 Seed = 0;
};

USTRUCT(BlueprintType)
struct FTimeThiefSmokeRuntimeSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke")
	FVector SmokeBoundsExtent = FVector(900.0f, 900.0f, 860.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float SmokeDuration = 12.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SmokeFadeOutDuration = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke", meta = (ClampMin = "8", UIMin = "8", ClampMax = "64", UIMax = "64"))
	int32 SmokeControlGridResolution = 32;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float InitialDensity = 0.725f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Plume", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float PlumeEmissionDuration = 2.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Plume", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float PlumeSourceRadius = 75.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Plume")
	float PlumeExpansionVelocity = 260.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Plume")
	float PlumeRiseVelocity = 95.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Plume", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SmokeBoundsExpansionSpeed = 90.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Obstacle")
	bool bUseStaticObstacleMask = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Obstacle", meta = (ClampMin = "8", UIMin = "8", ClampMax = "64", UIMax = "64"))
	int32 ObstacleMaskResolution = 32;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Obstacle", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ObstacleMaskInflation = 6.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Bounds Cells")
	bool bUseBoundsCellCluster = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Bounds Cells")
	FIntVector BoundsCellGrid = FIntVector(6, 6, 4);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Bounds Cells", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxActiveBoundsCells = 42;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Bounds Cells", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ExplosionBoundsShiftScale = 0.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|GPU", meta = (ClampMin = "16", UIMin = "16", ClampMax = "128", UIMax = "128"))
	int32 SmokeGridResolution = 64;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|GPU", meta = (ClampMin = "1", UIMin = "1", ClampMax = "64", UIMax = "64"))
	int32 PressureIterations = 10;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|GPU", meta = (ClampMin = "16", UIMin = "16", ClampMax = "128", UIMax = "128"))
	int32 RenderStepCount = 56;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|GPU", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Extinction = 2.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|GPU", meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "1.0", UIMax = "1.0"))
	float ScatteringAlbedo = 0.9f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|GPU", meta = (ClampMin = "-1.0", UIMin = "-1.0", ClampMax = "1.0", UIMax = "1.0"))
	float ScatteringAnisotropy = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|GPU", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float DensityDissipation = 0.014f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|GPU", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float VelocityDamping = 0.16f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|GPU", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float VorticityStrength = 0.65f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|GPU")
	bool bUseMacCormackAdvection = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|GPU", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxGPUEventsPerSmokePerFrame = 96;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Carrier Particles", meta = (ClampMin = "1", UIMin = "1", ClampMax = "128", UIMax = "128"))
	int32 CarrierParticleCount = 40;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Carrier Particles", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float CarrierParticleRadius = 92.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Carrier Particles", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float CarrierParticleDriftSpeed = 55.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Carrier Particles", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float CarrierParticleInteractionStrength = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Bullet", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float BulletClearRadius = 34.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Bullet", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float BulletWakeSampleSpacing = 85.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Bullet", meta = (ClampMin = "0.05", UIMin = "0.05"))
	float BulletWakeMaxVisibleLife = 2.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Explosion", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float ExplosionShockRadius = 420.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Explosion", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float ExplosionImpulseDuration = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Explosion")
	float ExplosionOutwardStrength = 900.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Explosion", meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "1.0", UIMax = "1.0"))
	float ExplosionDensityClearStrength = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Actor", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float ActorInteractionHz = 15.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Actor", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ActorPushVelocityThreshold = 80.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Actor", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxActorInteractionEventsPerTick = 12;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Debug")
	bool bDrawDebugBounds = false;
};
