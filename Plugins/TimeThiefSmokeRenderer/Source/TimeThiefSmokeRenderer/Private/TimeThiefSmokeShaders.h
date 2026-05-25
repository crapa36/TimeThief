#pragma once

#include "GlobalShader.h"
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
	FVector4f GridResolution_UseSparse = FVector4f::Zero();
	FVector4f BrickGridResolution_SmokeBrickSize = FVector4f::Zero();
	FVector4f SparseAtlasBrickGridResolution_MaxActive = FVector4f::Zero();
	FVector4f RenderSteps_EventsQuality = FVector4f::Zero();
	FVector4f AnalyticEvents = FVector4f::Zero();
	FVector4f AnalyticBulletControls = FVector4f::Zero();
	FVector4f RaymarchControls = FVector4f::Zero();
};

struct FTimeThiefSmokeCompositeTileRangeShaderData
{
	FVector2f OffsetCount = FVector2f::Zero();
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

class FTimeThiefSmokeBuildVortexBrickMasksCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeBuildVortexBrickMasksCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeBuildVortexBrickMasksCS, FGlobalShader);

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

class FTimeThiefSmokeScatterSparseAtlasCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeScatterSparseAtlasCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeScatterSparseAtlasCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		TIME_THIEF_SMOKE_SCATTER_SPARSE_ATLAS_CS_PARAMETERS
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeDivergenceCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeDivergenceCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeDivergenceCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		TIME_THIEF_SMOKE_DIVERGENCE_CS_PARAMETERS
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeBuildMacVelocityCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeBuildMacVelocityCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeBuildMacVelocityCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		TIME_THIEF_SMOKE_BUILD_MAC_VELOCITY_CS_PARAMETERS
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeMacDivergenceCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeMacDivergenceCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeMacDivergenceCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		TIME_THIEF_SMOKE_MAC_DIVERGENCE_CS_PARAMETERS
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeWarpCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeWarpCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeWarpCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		TIME_THIEF_SMOKE_WARP_CS_PARAMETERS
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

class FTimeThiefSmokePressureResidualCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokePressureResidualCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokePressureResidualCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		TIME_THIEF_SMOKE_PRESSURE_RESIDUAL_CS_PARAMETERS
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokePressureRestrictCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokePressureRestrictCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokePressureRestrictCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		TIME_THIEF_SMOKE_PRESSURE_RESTRICT_CS_PARAMETERS
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokePressureProlongateAddCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokePressureProlongateAddCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokePressureProlongateAddCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		TIME_THIEF_SMOKE_PRESSURE_PROLONGATE_ADD_CS_PARAMETERS
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeProjectVelocityCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeProjectVelocityCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeProjectVelocityCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		TIME_THIEF_SMOKE_PROJECT_VELOCITY_CS_PARAMETERS
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeProjectMacVelocityCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeProjectMacVelocityCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeProjectMacVelocityCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		TIME_THIEF_SMOKE_PROJECT_MAC_VELOCITY_CS_PARAMETERS
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeMacToCollocatedVelocityCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeMacToCollocatedVelocityCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeMacToCollocatedVelocityCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		TIME_THIEF_SMOKE_MAC_TO_COLLOCATED_VELOCITY_CS_PARAMETERS
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeCompositePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeCompositePS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeCompositePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		TIME_THIEF_SMOKE_COMPOSITE_PS_PARAMETERS
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeCompositeMultiPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeCompositeMultiPS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeCompositeMultiPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FVector4f, SceneColorUVScaleBias)
		SHADER_PARAMETER(FIntRect, ViewRect)
		SHADER_PARAMETER(FIntPoint, TileRectMin)
		SHADER_PARAMETER(FIntPoint, TileGridSize)
		SHADER_PARAMETER(int32, CompositeTileSize)
		SHADER_PARAMETER(int32, SmokeSlotCount)
		SHADER_PARAMETER(FMatrix44f, InvViewProjection)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FTimeThiefSmokeCompositeDescriptorShaderData>, CompositeSmokeDescriptors)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FTimeThiefSmokeCompositeTileRangeShaderData>, TileSmokeRanges)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, TileSmokeIndices)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FTimeThiefSmokeEventShaderData>, Events)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneDepthTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, DensityTexture0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, DensityTexture1)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, DensityTexture2)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, DensityTexture3)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, DensityTexture4)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, DensityTexture5)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, DensityTexture6)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, DisplacedDensityTexture0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, DisplacedDensityTexture1)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, DisplacedDensityTexture2)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, DisplacedDensityTexture3)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, DisplacedDensityTexture4)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, DisplacedDensityTexture5)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, DisplacedDensityTexture6)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, WarpTexture0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, WarpTexture1)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, WarpTexture2)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, WarpTexture3)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, WarpTexture4)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, WarpTexture5)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, WarpTexture6)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, ObstacleTexture0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, ObstacleTexture1)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, ObstacleTexture2)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, ObstacleTexture3)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, ObstacleTexture4)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, ObstacleTexture5)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, ObstacleTexture6)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, BulletCutoutTexture0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, BulletCutoutTexture1)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, BulletCutoutTexture2)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, BulletCutoutTexture3)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, BulletCutoutTexture4)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, BulletCutoutTexture5)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, BulletCutoutTexture6)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, BulletSinkTexture0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, BulletSinkTexture1)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, BulletSinkTexture2)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, BulletSinkTexture3)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, BulletSinkTexture4)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, BulletSinkTexture5)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, BulletSinkTexture6)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<uint>, BrickOccupancyTexture0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<uint>, BrickOccupancyTexture1)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<uint>, BrickOccupancyTexture2)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<uint>, BrickOccupancyTexture3)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<uint>, BrickOccupancyTexture4)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<uint>, BrickOccupancyTexture5)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<uint>, BrickOccupancyTexture6)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, SparseFieldAtlasTexture0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, SparseFieldAtlasTexture1)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, SparseFieldAtlasTexture2)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, SparseFieldAtlasTexture3)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, SparseFieldAtlasTexture4)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, SparseFieldAtlasTexture5)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, SparseFieldAtlasTexture6)
		SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorSampler)
		SHADER_PARAMETER_SAMPLER(SamplerState, VolumeSampler)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};
