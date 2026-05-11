#pragma once

#include "CoreMinimal.h"

namespace TimeThiefSmokeTuning
{
	inline const FVector DefaultSmokeBoundsExtent(1300.0f, 1300.0f, 1100.0f);

	inline constexpr float DefaultSmokeDuration = 12.0f;
	inline constexpr float DefaultSmokeFadeOutDuration = 5.0f;
	inline constexpr int32 DefaultSmokeControlGridResolution = 32;
	inline constexpr float DefaultInitialDensity = 0.54f;

	inline constexpr float DefaultPlumeEmissionDuration = 2.8f;
	inline constexpr float DefaultPlumeSourceRadius = 75.0f;
	inline constexpr float DefaultPlumeExpansionVelocity = 260.0f;
	inline constexpr float DefaultPlumeRiseVelocity = 95.0f;

	inline constexpr bool bDefaultUseStaticObstacleMask = true;
	inline constexpr int32 DefaultObstacleMaskResolution = 32;
	inline constexpr float DefaultObstacleMaskInflation = 6.0f;

	inline constexpr int32 MinSmokeGridResolution = 16;
	inline constexpr int32 MaxSmokeGridResolution = 128;
	inline constexpr int32 DefaultSmokeGridResolution = 64;
	inline constexpr int32 DefaultPressureIterations = 10;
	inline constexpr int32 DefaultRenderStepCount = 56;
	inline constexpr float DefaultExtinction = 3.35f;
	inline constexpr float DefaultScatteringAlbedo = 0.68f;
	inline constexpr float DefaultScatteringAnisotropy = 0.22f;
	inline constexpr float DefaultDensityDissipation = 0.014f;
	inline constexpr float DefaultVelocityDamping = 0.16f;
	inline constexpr float DefaultVorticityStrength = 0.65f;
	inline constexpr bool bDefaultUseMacCormackAdvection = false;
	inline constexpr int32 DefaultMaxGPUEventsPerSmokePerFrame = 96;

	inline constexpr int32 DefaultCarrierParticleCount = 28;
	inline constexpr int32 MaxCarrierParticleCount = 128;
	inline constexpr float DefaultCarrierParticleRadius = 92.0f;
	inline constexpr float DefaultCarrierParticleDriftSpeed = 55.0f;
	inline constexpr float DefaultCarrierParticleInteractionStrength = 1.0f;

	inline constexpr float DefaultBulletClearRadius = 34.0f;
	inline constexpr float DefaultBulletWakeSampleSpacing = 85.0f;
	inline constexpr float DefaultBulletWakeMaxVisibleLife = 5.0f;
	inline constexpr float MinBulletWakeVisibleLife = 0.05f;
	inline constexpr int32 MaxBulletTracesPerSmokePerTick = 18;

	inline constexpr float DefaultExplosionShockRadius = 420.0f;
	inline constexpr float DefaultExplosionImpulseDuration = 0.35f;
	inline constexpr float DefaultExplosionOutwardStrength = 900.0f;
	inline constexpr float DefaultExplosionDensityClearStrength = 0.25f;

	inline constexpr float DefaultActorInteractionHz = 15.0f;
	inline constexpr float DefaultActorPushVelocityThreshold = 10.0f;
	inline constexpr int32 DefaultMaxActorInteractionEventsPerTick = 12;

	inline constexpr int32 MaxDebugEventCount = 128;
	inline constexpr float ObstacleMaskBlendDuration = 0.25f;
}
