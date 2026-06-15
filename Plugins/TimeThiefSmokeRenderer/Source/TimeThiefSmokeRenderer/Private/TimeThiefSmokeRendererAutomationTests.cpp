#include "TimeThiefSmokeRendererTypes.h"
#include "TimeThiefSmokeShaders.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTimeThiefSmokeRendererDefaultsAutomationTest,
	"TimeThief.Smoke.Renderer.Defaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTimeThiefSmokeRendererDefaultsAutomationTest::RunTest(const FString& Parameters)
{
	TestTrue(
		TEXT("Grid resolution default stays inside allocation limits"),
		TimeThiefSmokeParameterDefaults::SmokeGridResolution >= TimeThiefSmokeParameterDefaults::SmokeGridMinAxisResolution &&
		TimeThiefSmokeParameterDefaults::SmokeGridResolution <= TimeThiefSmokeParameterDefaults::SmokeGridMaxAxisResolution);
	TestTrue(
		TEXT("Pressure iteration default stays inside solver limits"),
		TimeThiefSmokeParameterDefaults::PressureIterations >= TimeThiefSmokeParameterDefaults::PressureIterationsMin &&
		TimeThiefSmokeParameterDefaults::PressureIterations <= TimeThiefSmokeParameterDefaults::PressureIterationsMax);
	TestTrue(
		TEXT("Brick size default stays inside sparse limits"),
		TimeThiefSmokeParameterDefaults::SmokeBrickSize >= TimeThiefSmokeParameterDefaults::SmokeBrickMinSize &&
		TimeThiefSmokeParameterDefaults::SmokeBrickSize <= TimeThiefSmokeParameterDefaults::SmokeBrickMaxSize);
	TestTrue(
		TEXT("Render step defaults stay inside renderer limits"),
		TimeThiefSmokeParameterDefaults::RenderStepCount >= TimeThiefSmokeParameterDefaults::RenderStepCountMin &&
		TimeThiefSmokeParameterDefaults::RenderStepCount <= TimeThiefSmokeParameterDefaults::RenderStepCountMax &&
		TimeThiefSmokeParameterDefaults::RenderMaxStepCount >= TimeThiefSmokeParameterDefaults::RenderMaxStepCountMin &&
		TimeThiefSmokeParameterDefaults::RenderMaxStepCount <= TimeThiefSmokeParameterDefaults::RenderMaxStepCountMax);
	TestTrue(
		TEXT("Render voxel step default stays inside renderer limits"),
		TimeThiefSmokeParameterDefaults::RenderStepVoxelScale >= TimeThiefSmokeParameterDefaults::RenderStepVoxelScaleMin &&
		TimeThiefSmokeParameterDefaults::RenderStepVoxelScale <= TimeThiefSmokeParameterDefaults::RenderStepVoxelScaleMax);
	TestTrue(
		TEXT("Obstacle mask cache keeps at least one reusable entry"),
		TimeThiefSmokeParameterDefaults::ObstacleMaskCacheMaxEntries > 0);
	TestTrue(
		TEXT("Inactive brick raymarch skip keeps clamp range valid"),
		TimeThiefSmokeParameterDefaults::InactiveBrickRaymarchMaxSkipScale >= 1.0f);
	TestTrue(
		TEXT("Inactive brick self-shadow skip keeps clamp range valid"),
		TimeThiefSmokeParameterDefaults::SelfShadowInactiveBrickMaxSkipSteps >= 1);
	TestTrue(
		TEXT("Sparse composite active ratio remains normalized"),
		TimeThiefSmokeParameterDefaults::SparseCompositeMaxActiveRatio > 0.0f &&
		TimeThiefSmokeParameterDefaults::SparseCompositeMaxActiveRatio < 1.0f);
	TestTrue(
		TEXT("Dynamic obstacle refresh interval remains positive"),
		TimeThiefSmokeParameterDefaults::ObstacleDynamicRefreshIntervalSeconds > 0.0f);
	TestTrue(
		TEXT("Self-shadow sample threshold skips only tiny contributions"),
		TimeThiefSmokeParameterDefaults::SelfShadowMinSampleWeight > 0.0f &&
		TimeThiefSmokeParameterDefaults::SelfShadowMinSampleWeight < TimeThiefSmokeParameterDefaults::RenderTransmittanceEarlyOut);
	TestTrue(
		TEXT("Render transmittance early-out stays in normalized range"),
		TimeThiefSmokeParameterDefaults::RenderTransmittanceEarlyOut > 0.0f &&
		TimeThiefSmokeParameterDefaults::RenderTransmittanceEarlyOut < 1.0f);
	TestTrue(
		TEXT("Render boundary noise stays subtle enough to avoid banding"),
		TimeThiefSmokeParameterDefaults::RenderBoundaryNoiseScale >= 0.0f &&
		TimeThiefSmokeParameterDefaults::RenderBoundaryNoiseStrength >= 0.0f &&
		TimeThiefSmokeParameterDefaults::RenderBoundaryNoiseStrength <= 0.05f);
	TestTrue(
		TEXT("Actor airflow full speed leaves shader smoothstep range valid"),
		TimeThiefSmokeParameterDefaults::ActorAirflowFullSpeed >= TimeThiefSmokeParameterDefaults::ActorAirflowMinSpeed + TimeThiefSmokeParameterDefaults::ActorAirflowFullSpeedMinGap);
	TestTrue(
		TEXT("Actor push full response speed leaves smoke interaction range valid"),
		TimeThiefSmokeParameterDefaults::ActorPushFullResponseSpeed >
		TimeThiefSmokeParameterDefaults::ActorPushVelocityThreshold * TimeThiefSmokeParameterDefaults::ActorPushResponseStartSpeedScale);
	TestTrue(
		TEXT("Active impulse minimum duration remains positive"),
		TimeThiefSmokeParameterDefaults::ActiveImpulseMinDurationSeconds > 0.0f);
	TestTrue(
		TEXT("Shader event loop limit matches uploaded event buffer capacity"),
		TimeThiefSmokeParameterDefaults::ShaderEventLoopMaxCount == TimeThiefSmokeParameterDefaults::MaxShaderEventCount);
	TestTrue(
		TEXT("Simulation event delta clamp remains positive"),
		TimeThiefSmokeParameterDefaults::SimulationEventDeltaSecondsMax > 0.0f);
	TestTrue(
		TEXT("Smoke density clamp remains above initial density"),
		TimeThiefSmokeParameterDefaults::SmokeDensityMax >= TimeThiefSmokeParameterDefaults::InitialDensity);
	TestTrue(
		TEXT("Smoke velocity clamp remains positive"),
		TimeThiefSmokeParameterDefaults::MaxSmokeVelocity > 0.0f);
	TestTrue(
		TEXT("Actor wake trail length remains positive"),
		TimeThiefSmokeParameterDefaults::ActorWakeTrailLengthScale > 0.0f);
	TestTrue(
		TEXT("Actor wake roll force range remains ordered"),
		TimeThiefSmokeParameterDefaults::ActorWakeTrailMaxRollForce >= TimeThiefSmokeParameterDefaults::ActorWakeTrailMinRollForce);
	TestTrue(
		TEXT("Actor wake surface forces remain positive"),
		TimeThiefSmokeParameterDefaults::ActorWakeSurfaceRollForce > 0.0f &&
		TimeThiefSmokeParameterDefaults::ActorWakeSurfaceNoiseForce > 0.0f &&
		TimeThiefSmokeParameterDefaults::ActorWakeSurfaceTangentSpeedScale > 0.0f);
	TestTrue(
		TEXT("Actor wake street lane keeps a smooth inner radius"),
		TimeThiefSmokeParameterDefaults::ActorWakeStreetLaneInnerRadiusScale > 0.0f &&
		TimeThiefSmokeParameterDefaults::ActorWakeStreetLaneInnerRadiusScale < 1.0f);
	TestTrue(
		TEXT("Vorticity force defaults keep ordered positive force ranges"),
		TimeThiefSmokeParameterDefaults::AirInteractionRollBaseForce > 0.0f &&
		TimeThiefSmokeParameterDefaults::AirInteractionCurlNoiseForce > 0.0f &&
		TimeThiefSmokeParameterDefaults::VorticityConfinementForceScale > 0.0f &&
		TimeThiefSmokeParameterDefaults::TurbulenceBandMaxForce >= TimeThiefSmokeParameterDefaults::TurbulenceBandMinForce &&
		TimeThiefSmokeParameterDefaults::VorticityDeltaSpeedStrengthScale >= TimeThiefSmokeParameterDefaults::VorticityDeltaSpeedMin);
	TestTrue(
		TEXT("Vortex particle mask fits uint4 shader bitmask"),
		TimeThiefSmokeParameterDefaults::MaxVortexParticleCount <= 128);
	TestTrue(
		TEXT("Bullet wake life defaults stay above shader divisor minimum"),
		TimeThiefSmokeParameterDefaults::BulletWakeMaxVisibleLife >= TimeThiefSmokeParameterDefaults::BulletWakeMinLifeSeconds &&
		TimeThiefSmokeParameterDefaults::BulletWakeReleaseDuration >= TimeThiefSmokeParameterDefaults::BulletWakeMinLifeSeconds &&
		TimeThiefSmokeParameterDefaults::BulletWakeSinkLife >= TimeThiefSmokeParameterDefaults::BulletWakeMinLifeSeconds);
	TestTrue(
		TEXT("Bullet wake hold core keeps an ordered smoothstep range"),
		TimeThiefSmokeParameterDefaults::BulletWakeHoldCoreInnerRadiusScale > 0.0f &&
		TimeThiefSmokeParameterDefaults::BulletWakeHoldCoreOuterRadiusScale > TimeThiefSmokeParameterDefaults::BulletWakeHoldCoreInnerRadiusScale);
	TestTrue(
		TEXT("Fixed multi composite shader slot count matches static shader bindings"),
		TimeThiefSmokeParameterDefaults::MaxCompositeSmokeSlots == 3);
	TestTrue(
		TEXT("Renderer descriptor stays float4 aligned for HLSL structured buffer"),
		sizeof(FTimeThiefSmokeCompositeDescriptorShaderData) % sizeof(FVector4f) == 0);
	TestTrue(
		TEXT("Obstacle primitive upload stays float4 aligned for HLSL structured buffer"),
		sizeof(FTimeThiefSmokeObstaclePrimitive) % sizeof(FVector4f) == 0);

	return true;
}

#endif
