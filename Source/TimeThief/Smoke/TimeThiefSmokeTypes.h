#pragma once

#include "CoreMinimal.h"
#include "TimeThiefSmokeParameterDefaults.h"
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

	FTimeThiefSmokeRuntimeSettings();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke")
	FVector SmokeBoundsExtent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Render", meta = (ClampMin = "0.0", UIMin = "0.0"))
	FVector RenderBoundsPadding;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float SmokeDuration;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SmokeFadeOutDuration;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float InitialDensity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Plume", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float PlumeEmissionDuration;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Plume", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float PlumeSourceRadius;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Plume")
	float PlumeExpansionVelocity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Plume")
	float PlumeRiseVelocity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Obstacle")
	bool bUseStaticObstacleMask;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Obstacle", meta = (ClampMin = "8", UIMin = "8", ClampMax = "64", UIMax = "64"))
	int32 ObstacleMaskResolution;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Obstacle", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ObstacleMaskInflation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Bounds Cells")
	bool bUseBoundsCellCluster;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Bounds Cells")
	FIntVector BoundsCellGrid;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Bounds Cells", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxActiveBoundsCells;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Bounds Cells", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ExplosionBoundsShiftScale;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|GPU", meta = (ClampMin = "16", UIMin = "16", ClampMax = "128", UIMax = "128"))
	int32 SmokeGridResolution;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|GPU", meta = (ClampMin = "1", UIMin = "1", ClampMax = "64", UIMax = "64"))
	int32 PressureIterations;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|GPU", meta = (ClampMin = "16", UIMin = "16", ClampMax = "128", UIMax = "128"))
	int32 RenderStepCount;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|GPU", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Extinction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|GPU", meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "1.0", UIMax = "1.0"))
	float ScatteringAlbedo;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|GPU", meta = (ClampMin = "-1.0", UIMin = "-1.0", ClampMax = "1.0", UIMax = "1.0"))
	float ScatteringAnisotropy;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|GPU", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float DensityDissipation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|GPU", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float VelocityDamping;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|GPU", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float VorticityStrength;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|GPU", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float VorticityConfinementStrength;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|GPU", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float TurbulenceStrength;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|GPU", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float AirInteractionStrength;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|GPU", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float EventVortexStrength;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Warp", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float WarpTrailIntensity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Warp", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float WarpTrailDecayRate;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Warp", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float WarpTrailRadiusScale;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Warp", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float WarpTrailLengthScale;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|GPU")
	bool bUseMacCormackAdvection;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|GPU", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxGPUEventsPerSmokePerFrame;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Carrier Particles", meta = (ClampMin = "1", UIMin = "1", ClampMax = "128", UIMax = "128"))
	int32 CarrierParticleCount;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Carrier Particles", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float CarrierParticleRadius;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Carrier Particles", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float CarrierParticleDriftSpeed;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Carrier Particles", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float CarrierParticleInteractionStrength;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Bullet", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float BulletClearRadius;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Bullet", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float BulletWakeSampleSpacing;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Bullet", meta = (ClampMin = "0.05", UIMin = "0.05"))
	float BulletWakeMaxVisibleLife;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Explosion", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float ExplosionShockRadius;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Explosion", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float ExplosionImpulseDuration;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Explosion")
	float ExplosionOutwardStrength;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Explosion", meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "1.0", UIMax = "1.0"))
	float ExplosionDensityClearStrength;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Actor", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float ActorInteractionHz;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Actor", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ActorPushVelocityThreshold;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Actor", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxActorInteractionEventsPerTick;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Smoke|Debug")
	bool bDrawDebugBounds;
};
