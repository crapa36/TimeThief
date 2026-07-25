#pragma once

#include "CoreMinimal.h"

namespace TimeThiefSmokeParameterDefaults
{
	inline FVector GetSmokeBoundsExtent()
	{
		return FVector(700.0, 700.0, 400.0);
	}

	inline FVector GetRenderBoundsPadding()
	{
		return FVector(100.0, 100.0, 100.0);
	}

	constexpr float SmokeDuration = 30.0f;
	constexpr float SmokeFadeOutDuration = 5.0f;
	constexpr float InitialDensity = 5.0f;
	constexpr float SmokeDensityMax = 5.0f;

	constexpr float PlumeEmissionDuration = 2.5f;
	constexpr float PlumeSourceRadius = 100.0f;
	constexpr float PlumeExpansionVelocity = 500.0f;
	constexpr float PlumeRiseVelocity = 50.0f;

	// The obstacle field is deliberately at least as fine as the default fluid grid's longest axis.
	constexpr int32 ObstacleMaskResolution = 64;
	constexpr float ObstacleMaskInflation = 1.0f;
	constexpr float ObstacleMaskCellFootprintRatio = 0.25f;
	constexpr float ObstacleSourceClearRadiusScale = 1.5f;
	constexpr int32 MaxObstaclePrimitives = 32;
	constexpr float ObstacleFieldFarDistanceCm = 100000.0f;
	constexpr float ObstacleSdfSurfaceFeatherCm = 24.0f;
	constexpr float ObstacleSdfSurfaceFeatherCells = 1.25f;
	constexpr float ObstacleTransformLocationToleranceCm = 0.1f;
	constexpr float ObstacleTransformScaleTolerance = 0.001f;
	constexpr float ObstacleTransformRotationToleranceRadians = 0.001f;

	// 1400 x 1400 x 800 cm bounds resolve to 64 x 64 x 40 cells with the existing aspect-ratio sizing.
	constexpr float SmokeTargetVoxelSizeCm = 22.0f;
	constexpr float SmokeTargetVoxelSizeMinCm = 10.0f;
	constexpr float SmokeTargetVoxelSizeMaxCm = 100.0f;
	constexpr int32 SmokeGridResolution = 64;
	constexpr int32 SmokeGridMinAxisResolution = 4;
	constexpr int32 SmokeGridMaxAxisResolution = 128;
	constexpr int32 SmokeThreadGroupSize = 4;
	constexpr int32 SmokeGridAllocationQuantum = SmokeThreadGroupSize;

	// Accurate reference pressure path. A faster solver can be compared against this result.
	constexpr int32 PressureIterations = 40;
	constexpr int32 PressureIterationsMin = 1;
	constexpr int32 PressureIterationsMax = 80;

	constexpr int32 SmokeBrickSize = 8;
	constexpr int32 SmokeBrickMinSize = 4;
	constexpr int32 SmokeBrickMaxSize = 16;
	constexpr int32 MaxActiveSmokeBricks = 512;
	constexpr int32 SparseMinBrickGridAxis = 4;
	constexpr int32 SparseMinBrickCount = 64;
	constexpr float SparseVelocityActiveThreshold = 150.0f;
	constexpr float QualityCourantLimit = 1.0f;
	constexpr int32 MaxAdaptiveFluidSubsteps = 4;

	// Pressure solver: 0=reference Jacobi, 1=multigrid-preconditioned conjugate gradient.
	constexpr int32 PressureSolver = 1;
	constexpr int32 MGPCGMaxIterations = 16;
	constexpr int32 MGPCGPreSmoothIterations = 2;
	constexpr int32 MGPCGPostSmoothIterations = 2;
	constexpr int32 MGPCGCoarseIterations = 12;
	constexpr float MGPCGRelativeTolerance = 1.0e-4f;
	constexpr int32 VelocityReadbackIntervalFrames = 4;

	constexpr float RenderWorldStepLengthCm = 16.0f;
	constexpr float RenderWorldStepLengthMinCm = 10.0f;
	constexpr float RenderWorldStepLengthMaxCm = 200.0f;
	constexpr int32 HalfResolution = 1;
	constexpr int32 TemporalReconstruction = 1;
	constexpr float TemporalHistoryWeight = 0.88f;
	constexpr float TemporalDepthRejectionCm = 80.0f;
	constexpr float TemporalTransmittanceRejection = 0.18f;
	constexpr float TemporalNeighborhoodClampExpansion = 0.08f;
	constexpr int32 CompositeTileSize = 32;
	constexpr int32 MaxCompositeSmokeSlots = 8;
	constexpr int32 RenderOccupancyResolution = 4;
	constexpr float RenderOccupancyDensityThreshold = 0.0001f;
	constexpr int32 DetailNoiseResolution = 64;
	constexpr float CompositeFullscreenAreaThreshold = 0.58f;
	constexpr int32 CompositeScissorMinSavedPixels = 200000;
	constexpr int32 CompositeScreenRectPadding = 8;

	constexpr float Extinction = 3.0f;
	constexpr float ScatteringAnisotropy = 0.2f;
	inline FVector3f GetSelfShadowLightDirection()
	{
		return FVector3f(-0.45f, -0.25f, 0.86f).GetSafeNormal();
	}
	constexpr float SelfShadowStrength = 0.8f;
	constexpr float SelfShadowExtinction = 1.0f;
	constexpr int32 CombinedShadowStepCount = 32;
	constexpr int32 CombinedShadowStepCountMin = 0;
	constexpr int32 CombinedShadowStepCountMax = 32;
	constexpr float CombinedShadowStepLength = 48.0f;
	constexpr float CombinedShadowStepLengthMinCm = 10.0f;
	constexpr float CombinedShadowStepLengthMaxCm = 200.0f;
	constexpr float SelfShadowMinSampleWeight = 0.02f;
	constexpr float RenderTransmittanceEarlyOut = 0.02f;
	constexpr int32 ObstacleMaskCacheMaxEntries = 64;

	constexpr float RenderNoiseScale = 0.01f;
	constexpr float RenderNoiseStrength = 0.8f;
	constexpr float RenderNoiseTimeScale = 0.035f;
	constexpr float RenderBoundaryNoiseScale = 0.01f;
	constexpr float RenderBoundaryNoiseStrength = 0.16f;
	constexpr float RenderFilamentScale = 0.015f;
	constexpr float RenderFilamentStrength = 1.5f;
	constexpr float RenderFilamentContrast = 1.0f;
	constexpr float RenderFilamentWarpStrength = 1.5f;
	constexpr float RenderDetailDensityCutoff = 3.5f;

	constexpr float DensityDissipation = 0.02f;
	constexpr float VelocityDamping = 0.4f;

	constexpr float VorticityStrength = 1.0f;
	constexpr float VorticityConfinementStrength = 2.0f;
	constexpr float TurbulenceStrength = 1.0f;
	constexpr float AirInteractionStrength = 0.75f;
	constexpr float SelfWobbleTimeScale = 0.05f;
	constexpr float SelfWobbleVelocityScale = 0.25f;
	constexpr float SelfWobbleForceScale = 0.6f;
	constexpr float SelfWobbleParticleScale = 0.3f;
	constexpr float EventVortexStrength = 1.5f;
	constexpr int32 VortexParticleCount = 24;
	constexpr int32 MaxVortexParticleCount = 32;
	constexpr float VortexParticleLifeSeconds = 2.0f;
	constexpr float VortexParticleMinLifeSeconds = 0.05f;
	constexpr float VortexParticleStrength = 96.0f;
	constexpr float MaxSmokeVelocity = 2600.0f;
	constexpr float VortexParticleSplatRadius = 180.0f;
	constexpr float VortexParticleCoreRadius = 45.0f;
	constexpr float VortexParticleMinRadius = 1.0f;
	constexpr float VortexDensityGradientScale = 4.0f;
	constexpr float VortexSubstepIntervalSeconds = 1.0f / 8.0f;

	constexpr float ActorWakeTrailLengthScale = 12.0f;
	constexpr float ActorWakeStreetLaneInnerRadiusScale = 0.5f;
	constexpr float ActorWakeSurfaceRollForce = 500.0f;
	constexpr float ActorWakeSurfaceTangentSpeedScale = 0.5f;
	constexpr float ActorWakeSurfaceNoiseForce = 150.0f;
	constexpr float ActorWakeTrailMinRollForce = 250.0f;
	constexpr float ActorWakeTrailMaxRollForce = 650.0f;
	constexpr float ActorWakeStreetForceScale = 0.6f;
	constexpr float ActorWakeFrontPushScale = 0.2f;
	constexpr float AirInteractionRollBaseForce = 120.0f;
	constexpr float AirInteractionTangentialSpeedScale = 0.4f;
	constexpr float AirInteractionCurlNoiseForce = 200.0f;
	constexpr float VorticityConfinementForceScale = 25.0f;
	constexpr float TurbulenceBandMinForce = 180.0f;
	constexpr float TurbulenceBandMaxForce = 500.0f;
	constexpr float TurbulenceCurlMagnitudeScale = 0.2f;
	constexpr float AmbientAirCurlForce = 100.0f;
	constexpr float VorticityDeltaSpeedMin = 160.0f;
	constexpr float VorticityDeltaSpeedStrengthScale = 420.0f;

	constexpr float ActorAirflowStrength = 1.1f;
	constexpr float ActorAirflowMinSpeed = 10.0f;
	constexpr float ActorAirflowFullSpeed = 300.0f;
	constexpr float ActorAirflowRadiusScale = 1.0f;
	constexpr float ActorAirflowRadiusScaleMin = 0.1f;
	constexpr float ActorAirflowFullSpeedMinGap = 1.0f;
	constexpr float ActorAirflowFrontStrength = 2.2f;
	constexpr float ActorAirflowSideStrength = 0.3f;
	constexpr float ActorAirflowWakeStrength = 0.8f;
	constexpr float ActorAirflowVortexStrength = 0.35f;

	constexpr int32 MaxGPUEventsPerSmokePerFrame = 32;
	constexpr int32 MaxShaderEventCount = 48;
	constexpr int32 MaxSimulationBulletEventCount = 24;
	constexpr int32 MaxSimulationExplosionEventCount = 8;
	constexpr int32 MaxSimulationActorEventCount = 16;
	constexpr int32 MaxSimulationVortexEventCount = 16;
	constexpr float SimulationEventMinStrength = 0.001f;
	constexpr int32 ShaderEventLoopMaxCount = MaxShaderEventCount;
	constexpr float SimulationEventDeltaSecondsMax = 1.0f / 15.0f;
	constexpr int32 MaxBulletTracesPerSmokePerTick = 12;
	constexpr int32 MaxActiveExplosionImpulsesPerSmoke = 8;
	constexpr int32 MaxActorInteractionEventsPerTick = 16;

	constexpr float BulletClearRadius = 40.0f;
	constexpr float BulletClearRadiusRandomMin = 0.95f;
	constexpr float BulletClearRadiusRandomMax = 1.2f;
	constexpr float BulletWakeStrengthRandomMin = 0.9f;
	constexpr float BulletWakeStrengthRandomMax = 1.8f;
	constexpr float BulletWakeMaxVisibleLife = 0.5f;
	constexpr float BulletWakeReleaseDuration = 1.5f;
	constexpr float BulletWakeSinkLife = 0.2f;
	constexpr float BulletWakeSinkStrength = 0.25f;
	constexpr float BulletWakeImpulseStrength = 55.0f;
	constexpr float BulletWakeCutoutFeather = 1.0f;
	constexpr float BulletWakeMinLifeSeconds = 0.05f;
	constexpr float BulletWakeCutoutFeatherMin = 0.2f;
	constexpr float BulletWakeHoldCoreInnerRadiusScale = 0.25f;
	constexpr float BulletWakeHoldCoreOuterRadiusScale = 1.05f;

	constexpr float ExplosionShockRadius = 1.0f;
	constexpr float ExplosionImpulseDuration = 0.85f;
	constexpr float ExplosionInfluenceRadiusScale = 1.5f;
	constexpr float ExplosionOutwardStrength = 800.0f;

	constexpr float ActorInteractionHz = 10.0f;
	constexpr float ActorPushVelocityThreshold = 10.0f;
	constexpr float ActorPushResponseStartSpeedScale = 0.15f;
	constexpr float ActorPushFullResponseSpeed = 500.0f;
	constexpr float ActorPrimitiveRadiusScale = 0.45f;

	constexpr float SmokeSpatialCellSize = 2800.0f;
	constexpr int32 SmokeBroadphaseLinearScanMaxCount = 8;

	constexpr float SimulationHz = 30.0f;
	constexpr float SimulationFrameDeltaSecondsMax = 1.0f / 15.0f;
	constexpr int32 MaxSimulationSubstepsPerFrame = 4;

	constexpr float EventPriorityMinStrength = 0.01f;
	constexpr float EventPriorityMinAgeWeight = 0.05f;
	constexpr float EventPriorityRadiusDivisor = 200.0f;
	constexpr float EventPriorityRadiusMin = 0.5f;
	constexpr float EventPriorityRadiusMax = 4.0f;
	constexpr float ExplosionEventPriorityWeight = 1.55f;
	constexpr float ActorEventPriorityWeight = 1.3f;
	constexpr float PlumeEventPriorityWeight = 1.45f;
	constexpr float BulletEventPriorityWeight = 1.15f;
	constexpr float ActiveImpulseMinDurationSeconds = 0.01f;

	constexpr float BilateralDepthSensitivity = 10000.0f;
}
