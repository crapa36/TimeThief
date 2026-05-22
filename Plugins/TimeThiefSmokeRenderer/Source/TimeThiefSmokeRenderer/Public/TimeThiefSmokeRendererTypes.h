#pragma once

#include "CoreMinimal.h"
#include "TimeThiefSmokeParameterDefaults.h"

enum class ETimeThiefSmokeRendererInteractionType : uint8
{
	BulletWake = 0,
	ExplosionShock = 1,
	ActorPush = 2,
	PlumeSource = 3
};

enum class ETimeThiefSmokeRendererInteractionShape : uint8
{
	Sphere = 0,
	Capsule = 1,
	Box = 2,
	LineWake = 3
};

enum class ETimeThiefSmokeSimulationBackend : uint8
{
	DenseLegacy = 0,
	SparseMac = 1
};

enum class ETimeThiefSmokePressureSolver : uint8
{
	JacobiLegacy = 0,
	Multigrid = 1
};

struct TIMETHIEFSMOKERENDERER_API FTimeThiefSmokeRendererSettings
{
	FTimeThiefSmokeRendererSettings();

	ETimeThiefSmokeSimulationBackend SimulationBackend;
	ETimeThiefSmokePressureSolver PressureSolver;
	int32 SmokeGridResolution;
	int32 PressureIterations;
	int32 RenderStepCount;
	int32 SmokeBrickSize;
	int32 MaxActiveSmokeBricks;
	int32 RenderMaxStepCount;
	float RenderStepVoxelScale;
	int32 MaxGPUEventsPerSmokePerFrame;
	float InitialDensity;
	float SmokeFadeOutDuration;
	float PlumeEmissionDuration;
	float PlumeSourceRadius;
	float ObstacleSourceClearRadiusScale;
	float PlumeExpansionVelocity;
	float PlumeRiseVelocity;
	float Extinction;
	float ScatteringAlbedo;
	float ScatteringAnisotropy;
	FVector3f SelfShadowLightDirection;
	float SelfShadowStrength;
	float SelfShadowExtinction;
	int32 SelfShadowStepCount;
	float SelfShadowStepLength;
	float DensityDissipation;
	float VelocityDamping;
	float VorticityStrength;
	float VorticityConfinementStrength;
	float TurbulenceStrength;
	float AirInteractionStrength;
	float SelfWobbleTimeScale;
	float SelfWobbleVelocityScale;
	float SelfWobbleForceScale;
	float SelfWobbleParticleScale;
	float EventVortexStrength;
	int32 VortexParticleCount;
	float VortexParticleLifeSeconds;
	float VortexParticleStrength;
	float VortexParticleSplatRadius;
	float VortexParticleCoreRadius;
	float VortexDensityGradientScale;
	float WarpTrailIntensity;
	float WarpTrailDecayRate;
	float WarpTrailRadiusScale;
	float WarpTrailLengthScale;
	float ActorWarpDensityAccumulationScale;
	float ActorWarpAccumulationDecaySeconds;
	float ActorWarpEmissionRemainder;
	float ActorAirflowStrength;
	float ActorAirflowMinSpeed;
	float ActorAirflowFullSpeed;
	float ActorAirflowRadiusScale;
	float ActorAirflowFrontStrength;
	float ActorAirflowSideStrength;
	float ActorAirflowWakeStrength;
	float ActorAirflowVortexStrength;
	float SparseVelocityActiveThreshold;
	float BulletWakeMaxVisibleLife;
	float BulletWakeReleaseDuration;
	float BulletWakeSinkLife;
	float BulletWakeSinkStrength;
	float BulletWakeImpulseStrength;
	float BulletWakeCutoutFeather;
	bool bUseMacCormackAdvection;
	bool bUseAdaptiveMacCormack;
	bool bUseVortexBrickBins;
	float RenderNoiseScale;
	float RenderNoiseStrength;
	float RenderNoiseTimeScale;
	float RenderFilamentScale;
	float RenderFilamentStrength;
	float RenderFilamentContrast;
	float RenderFilamentWarpStrength;
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
	float Strength = 1.0f;
	float Speed = 0.0f;
	float WarpBudget = 0.0f;
	float NormalizedAge = 0.0f;
	int32 Seed = 0;
};

struct TIMETHIEFSMOKERENDERER_API FTimeThiefSmokeRendererVolume
{
	FTimeThiefSmokeRendererVolume();

	int32 SmokeId = INDEX_NONE;
	FTransform3f LocalToWorld = FTransform3f::Identity;
	FVector3f BoundsExtent;
	FVector3f SimulationBoundsExtent;
	FVector3f NaturalBoundsExtent;
	FVector3f RenderBoundsExtent;
	int32 ClusterId = INDEX_NONE;
	int32 ClusterSourceCount = 1;
	float AgeSeconds = 0.0f;
	float DurationSeconds;
	int32 ObstacleMaskResolution = 0;
	uint32 ObstacleMaskRevision = 0;
	TSharedPtr<const TArray<uint8>, ESPMode::ThreadSafe> ObstacleMask;
	bool bHasSolidObstacleMask = false;
	TArray<FTimeThiefSmokeRendererEvent> SourceEvents;
	FTimeThiefSmokeRendererSettings Settings;
};

struct TIMETHIEFSMOKERENDERER_API FTimeThiefSmokeRendererFrame
{
	float DeltaSeconds = 0.0f;
	TArray<FTimeThiefSmokeRendererVolume> Volumes;
	TArray<FTimeThiefSmokeRendererEvent> Events;
};
