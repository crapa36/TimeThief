#pragma once

#include "CoreMinimal.h"
#include "TimeThiefSmokeTuning.h"

enum class ETimeThiefSmokeRendererInteractionType : uint8
{
	BulletWake = 0,
	ExplosionShock = 1,
	ActorPush = 2
};

enum class ETimeThiefSmokeRendererInteractionShape : uint8
{
	Sphere = 0,
	Capsule = 1,
	Box = 2,
	LineWake = 3
};

struct TIMETHIEFSMOKERENDERER_API FTimeThiefSmokeRendererSettings
{
	int32 SmokeGridResolution = TimeThiefSmokeTuning::DefaultSmokeGridResolution;
	int32 PressureIterations = TimeThiefSmokeTuning::DefaultPressureIterations;
	int32 RenderStepCount = TimeThiefSmokeTuning::DefaultRenderStepCount;
	int32 MaxGPUEventsPerSmokePerFrame = TimeThiefSmokeTuning::DefaultMaxGPUEventsPerSmokePerFrame;
	float InitialDensity = TimeThiefSmokeTuning::DefaultInitialDensity;
	float SmokeFadeOutDuration = TimeThiefSmokeTuning::DefaultSmokeFadeOutDuration;
	float PlumeEmissionDuration = TimeThiefSmokeTuning::DefaultPlumeEmissionDuration;
	float PlumeSourceRadius = TimeThiefSmokeTuning::DefaultPlumeSourceRadius;
	float PlumeExpansionVelocity = TimeThiefSmokeTuning::DefaultPlumeExpansionVelocity;
	float PlumeRiseVelocity = TimeThiefSmokeTuning::DefaultPlumeRiseVelocity;
	float Extinction = TimeThiefSmokeTuning::DefaultExtinction;
	float ScatteringAlbedo = TimeThiefSmokeTuning::DefaultScatteringAlbedo;
	float ScatteringAnisotropy = TimeThiefSmokeTuning::DefaultScatteringAnisotropy;
	float DensityDissipation = TimeThiefSmokeTuning::DefaultDensityDissipation;
	float VelocityDamping = TimeThiefSmokeTuning::DefaultVelocityDamping;
	float VorticityStrength = TimeThiefSmokeTuning::DefaultVorticityStrength;
	float BulletWakeMaxVisibleLife = TimeThiefSmokeTuning::DefaultBulletWakeMaxVisibleLife;
	bool bUseMacCormackAdvection = TimeThiefSmokeTuning::bDefaultUseMacCormackAdvection;
	int32 CarrierParticleCount = TimeThiefSmokeTuning::DefaultCarrierParticleCount;
	float CarrierParticleRadius = TimeThiefSmokeTuning::DefaultCarrierParticleRadius;
	float CarrierParticleDriftSpeed = TimeThiefSmokeTuning::DefaultCarrierParticleDriftSpeed;
	float CarrierParticleInteractionStrength = TimeThiefSmokeTuning::DefaultCarrierParticleInteractionStrength;
};

struct TIMETHIEFSMOKERENDERER_API FTimeThiefSmokeRendererEvent
{
	int32 SmokeId = INDEX_NONE;
	ETimeThiefSmokeRendererInteractionType Type = ETimeThiefSmokeRendererInteractionType::BulletWake;
	ETimeThiefSmokeRendererInteractionShape Shape = ETimeThiefSmokeRendererInteractionShape::Sphere;
	FVector3f Position = FVector3f::ZeroVector;
	FVector3f PreviousPosition = FVector3f::ZeroVector;
	FVector3f Direction = FVector3f::ForwardVector;
	FQuat4f Rotation = FQuat4f::Identity;
	FVector3f Extents = FVector3f::ZeroVector;
	float Radius = 0.0f;
	float Length = 0.0f;
	float Speed = 0.0f;
	float Strength = 1.0f;
	float NormalizedAge = 0.0f;
	int32 Seed = 0;
};

struct TIMETHIEFSMOKERENDERER_API FTimeThiefSmokeRendererVolume
{
	int32 SmokeId = INDEX_NONE;
	FTransform3f LocalToWorld = FTransform3f::Identity;
	FVector3f BoundsExtent = FVector3f(TimeThiefSmokeTuning::DefaultSmokeBoundsExtent);
	float AgeSeconds = 0.0f;
	float DurationSeconds = 12.0f;
	int32 ObstacleMaskResolution = 0;
	uint32 ObstacleMaskRevision = 0;
	TArray<uint8> ObstacleMask;
	FTimeThiefSmokeRendererSettings Settings;
};

struct TIMETHIEFSMOKERENDERER_API FTimeThiefSmokeRendererFrame
{
	float DeltaSeconds = 0.0f;
	TArray<FTimeThiefSmokeRendererVolume> Volumes;
	TArray<FTimeThiefSmokeRendererEvent> Events;
};
