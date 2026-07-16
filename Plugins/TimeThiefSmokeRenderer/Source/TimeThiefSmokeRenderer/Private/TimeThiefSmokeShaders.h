#pragma once

#include "GlobalShader.h"
#include "ShaderPermutation.h"
#include "ShaderParameterStruct.h"
#include "TimeThiefSmokeShaderParameterMacros.h"
#include "TimeThiefSmokeRendererTypes.h"

struct FTimeThiefSmokeEventShaderData
{
	FVector4f PositionRadius = FVector4f::Zero();
	FVector4f DirectionLength = FVector4f::Zero();
	FVector4f ExtentsStrength = FVector4f::Zero();
	FVector4f Rotation = FVector4f::Zero();
	FVector4f TypeShapeAgeSeed = FVector4f::Zero();
	FVector4f PreviousPositionSpeed = FVector4f::Zero();
};

struct FTimeThiefSmokeVortexParticleShaderData
{
	FVector4f LocalPositionLife = FVector4f::Zero();
	FVector4f VelocityStrength = FVector4f::Zero();
	FVector4f AxisSeed = FVector4f(0.0f, 0.0f, 1.0f, 0.0f);
};

struct FTimeThiefSmokeCompositeDescriptorShaderData
{
	FVector4f WorldToLocal0 = FVector4f::Zero();
	FVector4f WorldToLocal1 = FVector4f::Zero();
	FVector4f WorldToLocal2 = FVector4f::Zero();
	FVector4f WorldToLocal3 = FVector4f::Zero();
	FVector4f BoundsExtent_RenderStepVoxelScale = FVector4f::Zero();
	FVector4f RenderBoundsExtent_Extinction = FVector4f::Zero();
	FVector4f ScatterNoise = FVector4f::Zero();
	FVector4f SelfShadowLightDirection_StepCount = FVector4f::Zero();
	FVector4f SelfShadowControls = FVector4f::Zero();
	FVector4f NoiseFilamentA = FVector4f::Zero();
	FVector4f FilamentAge = FVector4f::Zero();
	FVector4f RenderSteps_Quality = FVector4f::Zero();
	FVector4f NaturalBoundsExtent_ObstacleFeather = FVector4f::Zero();
	FVector4f RaymarchControls = FVector4f::Zero();
	FVector4f BoundaryNoiseControls = FVector4f::Zero();
};

struct FTimeThiefSmokeCompositeTileRangeShaderData
{
	FVector2f OffsetCount = FVector2f::Zero();
};

class FTimeThiefSmokeTestReduceCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeTestReduceCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeTestReduceCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FIntVector, GridResolution)
		SHADER_PARAMETER(FVector3f, BoundsExtent)
		SHADER_PARAMETER(float, SmokeDensityMax)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, DensityTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, DisplacedDensityTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, VelocityTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, BulletCutoutTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, BulletSinkTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, ObstacleTexture)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float4>, OutResult)
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeInitCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeInitCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeInitCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		TIME_THIEF_SMOKE_INIT_CS_PARAMETERS
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeBuildObstacleFieldCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeBuildObstacleFieldCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeBuildObstacleFieldCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FIntVector, GridResolution)
		SHADER_PARAMETER(FVector3f, BoundsExtent)
		SHADER_PARAMETER(int32, ObstaclePrimitiveCount)
		SHADER_PARAMETER(float, FarDistanceCm)
		SHADER_PARAMETER(float, SurfaceFeatherCm)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FTimeThiefSmokeObstaclePrimitive>, ObstaclePrimitives)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float>, OutObstacleSdfTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, OutObstacleVelocityTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, OutObstacleFaceOpenTexture)
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeApplyEventsCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeApplyEventsCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeApplyEventsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		TIME_THIEF_SMOKE_APPLY_EVENTS_CS_PARAMETERS
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeDynamicObstacleCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeDynamicObstacleCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeDynamicObstacleCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		TIME_THIEF_SMOKE_DYNAMIC_OBSTACLE_CS_PARAMETERS
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeSimulateCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeSimulateCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeSimulateCS, FGlobalShader);
	class FCompileBulletFieldsDim : SHADER_PERMUTATION_BOOL("TIME_THIEF_SIMULATE_COMPILE_BULLET_FIELDS");
	using FPermutationDomain = TShaderPermutationDomain<FCompileBulletFieldsDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		TIME_THIEF_SMOKE_SIMULATE_CS_PARAMETERS
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeVorticityCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeVorticityCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeVorticityCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		TIME_THIEF_SMOKE_VORTICITY_CS_PARAMETERS
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeBuildCurlCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeBuildCurlCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeBuildCurlCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		TIME_THIEF_SMOKE_BUILD_CURL_CS_PARAMETERS
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeUpdateVortexParticlesCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeUpdateVortexParticlesCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeUpdateVortexParticlesCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		TIME_THIEF_SMOKE_UPDATE_VORTEX_PARTICLES_CS_PARAMETERS
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeBuildVortexBrickMasksReverseCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeBuildVortexBrickMasksReverseCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeBuildVortexBrickMasksReverseCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		TIME_THIEF_SMOKE_BUILD_VORTEX_BRICK_MASKS_CS_PARAMETERS
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeSplatVortexParticlesCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeSplatVortexParticlesCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeSplatVortexParticlesCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		TIME_THIEF_SMOKE_SPLAT_VORTEX_PARTICLES_CS_PARAMETERS
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeBuildBrickOccupancyCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeBuildBrickOccupancyCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeBuildBrickOccupancyCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		TIME_THIEF_SMOKE_BUILD_BRICK_OCCUPANCY_CS_PARAMETERS
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeBuildEventBrickMasksCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeBuildEventBrickMasksCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeBuildEventBrickMasksCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		TIME_THIEF_SMOKE_BUILD_EVENT_BRICK_MASKS_CS_PARAMETERS
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeExpandBrickOccupancyCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeExpandBrickOccupancyCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeExpandBrickOccupancyCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		TIME_THIEF_SMOKE_EXPAND_BRICK_OCCUPANCY_CS_PARAMETERS
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokePackDenseFieldCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokePackDenseFieldCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokePackDenseFieldCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FIntVector, GridResolution)
		SHADER_PARAMETER(uint32, bPackBulletChannels)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, DensityTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, DisplacedDensityTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, BulletCutoutTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, BulletSinkTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, OutPackedDenseField)
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeBuildActiveBrickListCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeBuildActiveBrickListCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeBuildActiveBrickListCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		TIME_THIEF_SMOKE_BUILD_ACTIVE_BRICK_LIST_CS_PARAMETERS
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeBuildSparseScatterArgsCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeBuildSparseScatterArgsCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeBuildSparseScatterArgsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		TIME_THIEF_SMOKE_BUILD_SPARSE_SCATTER_ARGS_CS_PARAMETERS
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeBuildMacDivergenceCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeBuildMacDivergenceCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeBuildMacDivergenceCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		TIME_THIEF_SMOKE_BUILD_MAC_DIVERGENCE_CS_PARAMETERS
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokePressureJacobiCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokePressureJacobiCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokePressureJacobiCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		TIME_THIEF_SMOKE_PRESSURE_JACOBI_CS_PARAMETERS
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeProjectMacToCollocatedVelocityCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeProjectMacToCollocatedVelocityCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeProjectMacToCollocatedVelocityCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		TIME_THIEF_SMOKE_PROJECT_MAC_TO_COLLOCATED_VELOCITY_CS_PARAMETERS
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeCompositeMultiPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeCompositeMultiPS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeCompositeMultiPS, FGlobalShader);

	class FSingleSmokeDim : SHADER_PERMUTATION_BOOL("TIME_THIEF_COMPOSITE_SINGLE_SMOKE");
	class FStepStatsDim : SHADER_PERMUTATION_BOOL("TIME_THIEF_COMPOSITE_STEP_STATS");
	using FPermutationDomain = TShaderPermutationDomain<FSingleSmokeDim, FStepStatsDim>;

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		const FPermutationDomain PermutationVector(Parameters.PermutationId);
		OutEnvironment.SetDefine(
			TEXT("TIME_THIEF_MAX_COMPOSITE_SMOKES"),
			PermutationVector.Get<FSingleSmokeDim>() ? 1 : TimeThiefSmokeParameterDefaults::MaxCompositeSmokeSlots);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FVector4f, SceneColorUVScaleBias)
		SHADER_PARAMETER(FVector4f, SceneDepthPixelScaleBias)
		SHADER_PARAMETER(FIntRect, ViewRect)
		SHADER_PARAMETER(FIntRect, SceneDepthViewRect)
		SHADER_PARAMETER(FIntPoint, TileRectMin)
		SHADER_PARAMETER(FIntPoint, TileGridSize)
		SHADER_PARAMETER(int32, CompositeTileSize)
		SHADER_PARAMETER(int32, SmokeSlotCount)
		SHADER_PARAMETER(float, RenderWorldStepLength)
		SHADER_PARAMETER(float, CombinedShadowStepLength)
		SHADER_PARAMETER(float, CombinedShadowStrength)
		SHADER_PARAMETER(float, CombinedShadowExtinction)
		SHADER_PARAMETER(FVector3f, CombinedShadowLightDirection)
		SHADER_PARAMETER(int32, CombinedShadowStepCount)
		SHADER_PARAMETER(float, SelfShadowMinSampleWeight)
		SHADER_PARAMETER(int32, CompositeDebugMode)
		SHADER_PARAMETER(FMatrix44f, InvViewProjection)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FTimeThiefSmokeCompositeDescriptorShaderData>, CompositeSmokeDescriptors)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FTimeThiefSmokeCompositeTileRangeShaderData>, TileSmokeRanges)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, TileSmokeIndices)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, CompositeStepStats)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneDepthTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, ObstacleTexture0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, ObstacleTexture1)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, ObstacleTexture2)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, ObstacleTexture3)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, ObstacleTexture4)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, ObstacleTexture5)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, ObstacleTexture6)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, ObstacleTexture7)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, PackedDenseFieldTexture0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, PackedDenseFieldTexture1)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, PackedDenseFieldTexture2)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, PackedDenseFieldTexture3)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, PackedDenseFieldTexture4)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, PackedDenseFieldTexture5)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, PackedDenseFieldTexture6)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, PackedDenseFieldTexture7)
		SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorSampler)
		SHADER_PARAMETER_SAMPLER(SamplerState, VolumeSampler)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeBilateralUpsamplePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeBilateralUpsamplePS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeBilateralUpsamplePS, FGlobalShader);
	class FDirectResolveDim : SHADER_PERMUTATION_BOOL("TIME_THIEF_SMOKE_DIRECT_RESOLVE");
	using FPermutationDomain = TShaderPermutationDomain<FDirectResolveDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FVector4f, SceneColorUVScaleBias)
		SHADER_PARAMETER(FVector4f, SceneDepthPixelScaleBias)
		SHADER_PARAMETER(FIntRect, ViewRect)
		SHADER_PARAMETER(FIntRect, SceneDepthViewRect)
		SHADER_PARAMETER(FVector2f, HalfResSize)
		SHADER_PARAMETER(float, BilateralDepthSensitivity)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, SceneDepthTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, HalfResSmokeTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorSampler)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};
