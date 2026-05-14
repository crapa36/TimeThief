#include "TimeThiefSmokeRendererTypes.h"

namespace
{
	FVector3f ToVector3f(const FVector& Value)
	{
		return FVector3f(static_cast<float>(Value.X), static_cast<float>(Value.Y), static_cast<float>(Value.Z));
	}

	FVector3f GetDefaultRenderBoundsExtent()
	{
		return ToVector3f(TimeThiefSmokeParameterDefaults::GetSmokeBoundsExtent() + TimeThiefSmokeParameterDefaults::GetRenderBoundsPadding());
	}
}

FTimeThiefSmokeRendererSettings::FTimeThiefSmokeRendererSettings()
	: SmokeGridResolution(TimeThiefSmokeParameterDefaults::SmokeGridResolution)
	, PressureIterations(TimeThiefSmokeParameterDefaults::PressureIterations)
	, RenderStepCount(TimeThiefSmokeParameterDefaults::RenderStepCount)
	, MaxGPUEventsPerSmokePerFrame(TimeThiefSmokeParameterDefaults::MaxGPUEventsPerSmokePerFrame)
	, InitialDensity(TimeThiefSmokeParameterDefaults::InitialDensity)
	, SmokeFadeOutDuration(TimeThiefSmokeParameterDefaults::SmokeFadeOutDuration)
	, PlumeEmissionDuration(TimeThiefSmokeParameterDefaults::PlumeEmissionDuration)
	, PlumeSourceRadius(TimeThiefSmokeParameterDefaults::PlumeSourceRadius)
	, PlumeExpansionVelocity(TimeThiefSmokeParameterDefaults::PlumeExpansionVelocity)
	, PlumeRiseVelocity(TimeThiefSmokeParameterDefaults::PlumeRiseVelocity)
	, Extinction(TimeThiefSmokeParameterDefaults::Extinction)
	, ScatteringAlbedo(TimeThiefSmokeParameterDefaults::ScatteringAlbedo)
	, ScatteringAnisotropy(TimeThiefSmokeParameterDefaults::ScatteringAnisotropy)
	, DensityDissipation(TimeThiefSmokeParameterDefaults::DensityDissipation)
	, VelocityDamping(TimeThiefSmokeParameterDefaults::VelocityDamping)
	, VorticityStrength(TimeThiefSmokeParameterDefaults::VorticityStrength)
	, VorticityConfinementStrength(TimeThiefSmokeParameterDefaults::VorticityConfinementStrength)
	, TurbulenceStrength(TimeThiefSmokeParameterDefaults::TurbulenceStrength)
	, AirInteractionStrength(TimeThiefSmokeParameterDefaults::AirInteractionStrength)
	, EventVortexStrength(TimeThiefSmokeParameterDefaults::EventVortexStrength)
	, WarpTrailIntensity(TimeThiefSmokeParameterDefaults::WarpTrailIntensity)
	, WarpTrailDecayRate(TimeThiefSmokeParameterDefaults::WarpTrailDecayRate)
	, WarpTrailRadiusScale(TimeThiefSmokeParameterDefaults::WarpTrailRadiusScale)
	, WarpTrailLengthScale(TimeThiefSmokeParameterDefaults::WarpTrailLengthScale)
	, BulletWakeMaxVisibleLife(TimeThiefSmokeParameterDefaults::BulletWakeMaxVisibleLife)
	, bUseMacCormackAdvection(TimeThiefSmokeParameterDefaults::bUseMacCormackAdvection)
	, CarrierParticleCount(TimeThiefSmokeParameterDefaults::CarrierParticleCount)
	, CarrierParticleRadius(TimeThiefSmokeParameterDefaults::CarrierParticleRadius)
	, CarrierParticleDriftSpeed(TimeThiefSmokeParameterDefaults::CarrierParticleDriftSpeed)
	, CarrierParticleInteractionStrength(TimeThiefSmokeParameterDefaults::CarrierParticleInteractionStrength)
{
}

FTimeThiefSmokeRendererVolume::FTimeThiefSmokeRendererVolume()
	: BoundsExtent(ToVector3f(TimeThiefSmokeParameterDefaults::GetSmokeBoundsExtent()))
	, RenderBoundsExtent(GetDefaultRenderBoundsExtent())
	, DurationSeconds(TimeThiefSmokeParameterDefaults::SmokeDuration)
{
}
