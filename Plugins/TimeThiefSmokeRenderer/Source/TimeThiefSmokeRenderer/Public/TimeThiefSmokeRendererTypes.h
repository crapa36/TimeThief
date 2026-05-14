#pragma once

#include "CoreMinimal.h"

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
	int32 SmokeGridResolution = 64;
	int32 PressureIterations = 10;
	int32 RenderStepCount = 56;
	int32 MaxGPUEventsPerSmokePerFrame = 96;
	float InitialDensity = 0.725f;
	float SmokeFadeOutDuration = 5.0f;
	float PlumeEmissionDuration = 2.8f;
	float PlumeSourceRadius = 75.0f;
	float PlumeExpansionVelocity = 260.0f;
	float PlumeRiseVelocity = 95.0f;
	float Extinction = 2.25f;
	float ScatteringAlbedo = 0.9f;
	float ScatteringAnisotropy = 0.35f;
	float DensityDissipation = 0.014f;
	float VelocityDamping = 0.16f;
	float VorticityStrength = 0.65f;
	float BulletWakeMaxVisibleLife = 2.5f;
	bool bUseMacCormackAdvection = false;
	int32 CarrierParticleCount = 40;
	float CarrierParticleRadius = 92.0f;
	float CarrierParticleDriftSpeed = 55.0f;
	float CarrierParticleInteractionStrength = 1.0f;
};

struct TIMETHIEFSMOKERENDERER_API FTimeThiefSmokeRendererEvent
{
	int32 SmokeId = INDEX_NONE;
	ETimeThiefSmokeRendererInteractionType Type = ETimeThiefSmokeRendererInteractionType::BulletWake;
	ETimeThiefSmokeRendererInteractionShape Shape = ETimeThiefSmokeRendererInteractionShape::Sphere;
	FVector3f Position = FVector3f::ZeroVector;
	FVector3f Direction = FVector3f::ForwardVector;
	FQuat4f Rotation = FQuat4f::Identity;
	FVector3f Extents = FVector3f::ZeroVector;
	float Radius = 0.0f;
	float Length = 0.0f;
	float Strength = 1.0f;
	float NormalizedAge = 0.0f;
	int32 Seed = 0;
};

struct TIMETHIEFSMOKERENDERER_API FTimeThiefSmokeRendererVolume
{
	int32 SmokeId = INDEX_NONE;
	FTransform3f LocalToWorld = FTransform3f::Identity;
	FVector3f BoundsExtent = FVector3f(900.0f, 900.0f, 860.0f);
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
