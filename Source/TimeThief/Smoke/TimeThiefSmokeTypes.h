#pragma once

#include "CoreMinimal.h"
#include "TimeThiefSmokeTuning.h"
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
	FVector SmokeBoundsExtent = TimeThiefSmokeTuning::DefaultSmokeBoundsExtent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float SmokeDuration = TimeThiefSmokeTuning::DefaultSmokeDuration;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SmokeFadeOutDuration = TimeThiefSmokeTuning::DefaultSmokeFadeOutDuration;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke", meta = (ClampMin = "8", UIMin = "8", ClampMax = "64", UIMax = "64"))
	int32 SmokeControlGridResolution = TimeThiefSmokeTuning::DefaultSmokeControlGridResolution;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float InitialDensity = TimeThiefSmokeTuning::DefaultInitialDensity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Plume", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float PlumeEmissionDuration = TimeThiefSmokeTuning::DefaultPlumeEmissionDuration;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Plume", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float PlumeSourceRadius = TimeThiefSmokeTuning::DefaultPlumeSourceRadius;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Plume")
	float PlumeExpansionVelocity = TimeThiefSmokeTuning::DefaultPlumeExpansionVelocity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Plume")
	float PlumeRiseVelocity = TimeThiefSmokeTuning::DefaultPlumeRiseVelocity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Obstacle")
	bool bUseStaticObstacleMask = TimeThiefSmokeTuning::bDefaultUseStaticObstacleMask;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Obstacle", meta = (ClampMin = "8", UIMin = "8", ClampMax = "64", UIMax = "64"))
	int32 ObstacleMaskResolution = TimeThiefSmokeTuning::DefaultObstacleMaskResolution;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Obstacle", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ObstacleMaskInflation = TimeThiefSmokeTuning::DefaultObstacleMaskInflation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|GPU", meta = (ClampMin = "16", UIMin = "16", ClampMax = "128", UIMax = "128"))
	int32 SmokeGridResolution = TimeThiefSmokeTuning::DefaultSmokeGridResolution;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|GPU", meta = (ClampMin = "1", UIMin = "1", ClampMax = "64", UIMax = "64"))
	int32 PressureIterations = TimeThiefSmokeTuning::DefaultPressureIterations;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|GPU", meta = (ClampMin = "16", UIMin = "16", ClampMax = "128", UIMax = "128"))
	int32 RenderStepCount = TimeThiefSmokeTuning::DefaultRenderStepCount;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|GPU", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Extinction = TimeThiefSmokeTuning::DefaultExtinction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|GPU", meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "1.0", UIMax = "1.0"))
	float ScatteringAlbedo = TimeThiefSmokeTuning::DefaultScatteringAlbedo;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|GPU", meta = (ClampMin = "-1.0", UIMin = "-1.0", ClampMax = "1.0", UIMax = "1.0"))
	float ScatteringAnisotropy = TimeThiefSmokeTuning::DefaultScatteringAnisotropy;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|GPU", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float DensityDissipation = TimeThiefSmokeTuning::DefaultDensityDissipation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|GPU", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float VelocityDamping = TimeThiefSmokeTuning::DefaultVelocityDamping;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|GPU", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float VorticityStrength = TimeThiefSmokeTuning::DefaultVorticityStrength;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|GPU")
	bool bUseMacCormackAdvection = TimeThiefSmokeTuning::bDefaultUseMacCormackAdvection;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|GPU", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxGPUEventsPerSmokePerFrame = TimeThiefSmokeTuning::DefaultMaxGPUEventsPerSmokePerFrame;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Carrier Particles", meta = (ClampMin = "1", UIMin = "1", ClampMax = "128", UIMax = "128"))
	int32 CarrierParticleCount = TimeThiefSmokeTuning::DefaultCarrierParticleCount;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Carrier Particles", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float CarrierParticleRadius = TimeThiefSmokeTuning::DefaultCarrierParticleRadius;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Carrier Particles", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float CarrierParticleDriftSpeed = TimeThiefSmokeTuning::DefaultCarrierParticleDriftSpeed;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Carrier Particles", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float CarrierParticleInteractionStrength = TimeThiefSmokeTuning::DefaultCarrierParticleInteractionStrength;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Bullet", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float BulletClearRadius = TimeThiefSmokeTuning::DefaultBulletClearRadius;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Bullet", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float BulletWakeSampleSpacing = TimeThiefSmokeTuning::DefaultBulletWakeSampleSpacing;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Bullet", meta = (ClampMin = "0.05", UIMin = "0.05"))
	float BulletWakeMaxVisibleLife = TimeThiefSmokeTuning::DefaultBulletWakeMaxVisibleLife;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Explosion", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float ExplosionShockRadius = TimeThiefSmokeTuning::DefaultExplosionShockRadius;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Explosion", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float ExplosionImpulseDuration = TimeThiefSmokeTuning::DefaultExplosionImpulseDuration;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Explosion")
	float ExplosionOutwardStrength = TimeThiefSmokeTuning::DefaultExplosionOutwardStrength;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Explosion", meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "1.0", UIMax = "1.0"))
	float ExplosionDensityClearStrength = TimeThiefSmokeTuning::DefaultExplosionDensityClearStrength;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Actor", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float ActorInteractionHz = TimeThiefSmokeTuning::DefaultActorInteractionHz;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Actor", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ActorPushVelocityThreshold = TimeThiefSmokeTuning::DefaultActorPushVelocityThreshold;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Actor", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxActorInteractionEventsPerTick = TimeThiefSmokeTuning::DefaultMaxActorInteractionEventsPerTick;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Debug")
	bool bDrawDebugBounds = false;
};
