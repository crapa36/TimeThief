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
	FVector4f ExplosionParameters = FVector4f(
		TimeThiefSmokeParameterDefaults::ExplosionInfluenceRadiusScale,
		0.0f,
		0.0f,
		0.0f);
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
	FVector4f BoundsExtent_DetailDensityCutoff = FVector4f::Zero();
	FVector4f RenderBoundsExtent_Extinction = FVector4f::Zero();
	FVector4f AnisotropyNoise = FVector4f::Zero();
	FVector4f SelfShadowLightDirection = FVector4f::Zero();
	FVector4f NoiseFilamentA = FVector4f::Zero();
	FVector4f FilamentAge = FVector4f::Zero();
	FVector4f NaturalBoundsExtent_ObstacleFeather = FVector4f::Zero();
	FVector4f BoundaryNoiseControls = FVector4f::Zero();
};

struct FTimeThiefSmokeCompositeTileRangeShaderData
{
	FVector2f OffsetCount = FVector2f::Zero();
};

class FTimeThiefSmokeTestReduceTilesCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeTestReduceTilesCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeTestReduceTilesCS, FGlobalShader);

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
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float4>, OutTileResults)
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeTestReduceFinalCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeTestReduceFinalCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeTestReduceFinalCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, TileCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, TileResults)
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

class FTimeThiefSmokeBuildRenderOccupancyCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeBuildRenderOccupancyCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeBuildRenderOccupancyCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FIntVector, GridResolution)
		SHADER_PARAMETER(FIntVector, MacroGridResolution)
		SHADER_PARAMETER(float, DensityThreshold)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, PackedDenseFieldTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, PreviousPackedDenseFieldTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<uint>, OutRenderOccupancy)
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeBuildDetailNoiseCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeBuildDetailNoiseCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeBuildDetailNoiseCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FIntVector, GridResolution)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, OutDetailNoise)
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeBuildExtinctionVolumeCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeBuildExtinctionVolumeCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeBuildExtinctionVolumeCS, FGlobalShader);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FIntVector, GridResolution)
		SHADER_PARAMETER(FVector3f, BoundsExtent)
		SHADER_PARAMETER(FVector3f, NaturalBoundsExtent)
		SHADER_PARAMETER(float, NoiseScale)
		SHADER_PARAMETER(float, NoiseStrength)
		SHADER_PARAMETER(float, NoiseTime)
		SHADER_PARAMETER(float, LifetimeAlpha)
		SHADER_PARAMETER(float, ObstacleFeather)
		SHADER_PARAMETER(FMatrix44f, LocalToWorld)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, PackedDenseFieldTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, ObstacleTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, DetailNoiseTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, VolumeSampler)
		SHADER_PARAMETER_SAMPLER(SamplerState, DetailNoiseSampler)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float>, OutExtinctionTexture)
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeBuildLightVolumeCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeBuildLightVolumeCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeBuildLightVolumeCS, FGlobalShader);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FIntVector, GridResolution)
		SHADER_PARAMETER(FVector3f, BoundsExtent)
		SHADER_PARAMETER(FVector3f, LightDirection)
		SHADER_PARAMETER(float, ShadowStepLength)
		SHADER_PARAMETER(int32, ShadowStepCount)
		SHADER_PARAMETER(FMatrix44f, WorldToLocal)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, ExtinctionTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, VolumeSampler)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float>, OutLightOpticalDepth)
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

class FTimeThiefSmokeVelocityMaxTilesCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeVelocityMaxTilesCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeVelocityMaxTilesCS, FGlobalShader);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FIntVector, GridResolution)
		SHADER_PARAMETER(FIntVector, GroupGridResolution)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, VelocityTexture)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float>, OutTileMaximums)
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeVelocityMaxFinalCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeVelocityMaxFinalCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeVelocityMaxFinalCS, FGlobalShader);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FIntVector, GroupGridResolution)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float>, TileMaximums)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float>, OutMaximum)
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokePressureResidualCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokePressureResidualCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokePressureResidualCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FIntVector, GridResolution)
		SHADER_PARAMETER(FIntVector, BrickGridResolution)
		SHADER_PARAMETER(FVector3f, CellSize)
		SHADER_PARAMETER(int32, SmokeBrickSize)
		SHADER_PARAMETER(uint32, bUseSparseSimulationMask)
		SHADER_PARAMETER(float, ObstacleSdfSurfaceFeatherCm)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, PressureIn)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, DivergenceIn)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, ObstacleTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, ObstacleFaceOpenTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<uint>, BrickOccupancyTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, VolumeSampler)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float>, OutResidual)
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeProjectionReduceTilesCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeProjectionReduceTilesCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeProjectionReduceTilesCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FIntVector, GridResolution)
		SHADER_PARAMETER(FIntVector, BrickGridResolution)
		SHADER_PARAMETER(FVector3f, CellSize)
		SHADER_PARAMETER(int32, SmokeBrickSize)
		SHADER_PARAMETER(uint32, bUseSparseSimulationMask)
		SHADER_PARAMETER(float, ObstacleSdfSurfaceFeatherCm)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, DivergenceBeforeTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, DivergenceAfterTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, PressureResidualTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, ProjectedVelocityTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, ObstacleTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, ObstacleVelocityTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, ObstacleFaceOpenTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<uint>, BrickOccupancyTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, VolumeSampler)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float4>, OutTileResults)
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeProjectionReduceFinalCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeProjectionReduceFinalCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeProjectionReduceFinalCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, TileCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, TileResults)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float4>, OutResult)
	END_SHADER_PARAMETER_STRUCT()
};


#define TIME_THIEF_DECLARE_MGPCG_SHADER(ShaderClass) \
class ShaderClass : public FGlobalShader \
{ \
public: \
	DECLARE_GLOBAL_SHADER(ShaderClass); \
	SHADER_USE_PARAMETER_STRUCT(ShaderClass, FGlobalShader); \
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, ) \
		SHADER_PARAMETER(FIntVector, GridResolution) \
		SHADER_PARAMETER(FIntVector, FineGridResolution) \
		SHADER_PARAMETER(FIntVector, GroupGridResolution) \
		SHADER_PARAMETER(FVector3f, CellSize) \
		SHADER_PARAMETER(float, RelativeTolerance) \
		SHADER_PARAMETER(int32, IterationIndex) \
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, DivergenceTexture) \
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, VectorA) \
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, VectorB) \
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, PressureTexture) \
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, RightHandSideTexture) \
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, FineTexture) \
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, CoarseTexture) \
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, ObstacleTexture) \
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, ObstacleFaceOpenTexture) \
		SHADER_PARAMETER_SAMPLER(SamplerState, VolumeSampler) \
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float>, InputScalar0) \
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float>, InputScalar1) \
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, SolverStateIn) \
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float>, TileSums) \
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float2>, MeanTileSums) \
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float>, OutVector) \
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float>, OutVectorA) \
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float>, OutVectorB) \
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float>, OutVectorC) \
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float>, OutVectorD) \
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float>, OutTileSums) \
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float2>, OutMeanTileSums) \
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float>, OutScalar) \
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float4>, OutSolverState) \
	END_SHADER_PARAMETER_STRUCT() \
};

TIME_THIEF_DECLARE_MGPCG_SHADER(FTimeThiefSmokeMGPCGMeanTilesCS)
TIME_THIEF_DECLARE_MGPCG_SHADER(FTimeThiefSmokeMGPCGMeanFinalCS)
TIME_THIEF_DECLARE_MGPCG_SHADER(FTimeThiefSmokeMGPCGInitializeCS)
TIME_THIEF_DECLARE_MGPCG_SHADER(FTimeThiefSmokeMGPCGApplyOperatorCS)
TIME_THIEF_DECLARE_MGPCG_SHADER(FTimeThiefSmokeMGPCGJacobiSmoothCS)
TIME_THIEF_DECLARE_MGPCG_SHADER(FTimeThiefSmokeMGPCGResidualCS)
TIME_THIEF_DECLARE_MGPCG_SHADER(FTimeThiefSmokeMGPCGRestrictCS)
TIME_THIEF_DECLARE_MGPCG_SHADER(FTimeThiefSmokeMGPCGProlongateAddCS)
TIME_THIEF_DECLARE_MGPCG_SHADER(FTimeThiefSmokeMGPCGCopyCS)
TIME_THIEF_DECLARE_MGPCG_SHADER(FTimeThiefSmokeMGPCGUpdateXRCS)
TIME_THIEF_DECLARE_MGPCG_SHADER(FTimeThiefSmokeMGPCGUpdatePCS)
TIME_THIEF_DECLARE_MGPCG_SHADER(FTimeThiefSmokeMGPCGDotTilesCS)
TIME_THIEF_DECLARE_MGPCG_SHADER(FTimeThiefSmokeMGPCGDotFinalCS)
TIME_THIEF_DECLARE_MGPCG_SHADER(FTimeThiefSmokeMGPCGInitializeStateCS)
TIME_THIEF_DECLARE_MGPCG_SHADER(FTimeThiefSmokeMGPCGComputeAlphaCS)
TIME_THIEF_DECLARE_MGPCG_SHADER(FTimeThiefSmokeMGPCGComputeBetaCS)

#undef TIME_THIEF_DECLARE_MGPCG_SHADER

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
	class FCostModeDim : SHADER_PERMUTATION_INT("TIME_THIEF_COMPOSITE_COST_MODE", 5);
	using FPermutationDomain = TShaderPermutationDomain<FSingleSmokeDim, FStepStatsDim, FCostModeDim>;

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
		SHADER_PARAMETER(float, SamplePhase)
		SHADER_PARAMETER(int32, CompositeDebugMode)
		SHADER_PARAMETER(FIntVector, RenderOccupancyResolution)
		SHADER_PARAMETER(FMatrix44f, InvViewProjection)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FTimeThiefSmokeCompositeDescriptorShaderData>, CompositeSmokeDescriptors)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FTimeThiefSmokeCompositeTileRangeShaderData>, TileSmokeRanges)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, TileSmokeIndices)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, CompositeStepStats)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneDepthTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, PackedDenseFieldTexture0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, PackedDenseFieldTexture1)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, PackedDenseFieldTexture2)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, PackedDenseFieldTexture3)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, PackedDenseFieldTexture4)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, PackedDenseFieldTexture5)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, PackedDenseFieldTexture6)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, PackedDenseFieldTexture7)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, PreviousPackedDenseFieldTexture0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, PreviousPackedDenseFieldTexture1)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, PreviousPackedDenseFieldTexture2)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, PreviousPackedDenseFieldTexture3)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, PreviousPackedDenseFieldTexture4)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, PreviousPackedDenseFieldTexture5)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, PreviousPackedDenseFieldTexture6)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, PreviousPackedDenseFieldTexture7)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, ExtinctionTexture0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, ExtinctionTexture1)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, ExtinctionTexture2)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, ExtinctionTexture3)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, ExtinctionTexture4)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, ExtinctionTexture5)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, ExtinctionTexture6)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, ExtinctionTexture7)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, PreviousExtinctionTexture0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, PreviousExtinctionTexture1)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, PreviousExtinctionTexture2)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, PreviousExtinctionTexture3)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, PreviousExtinctionTexture4)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, PreviousExtinctionTexture5)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, PreviousExtinctionTexture6)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, PreviousExtinctionTexture7)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, LightOpticalDepthTexture0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, LightOpticalDepthTexture1)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, LightOpticalDepthTexture2)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, LightOpticalDepthTexture3)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, LightOpticalDepthTexture4)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, LightOpticalDepthTexture5)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, LightOpticalDepthTexture6)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, LightOpticalDepthTexture7)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, PreviousLightOpticalDepthTexture0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, PreviousLightOpticalDepthTexture1)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, PreviousLightOpticalDepthTexture2)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, PreviousLightOpticalDepthTexture3)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, PreviousLightOpticalDepthTexture4)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, PreviousLightOpticalDepthTexture5)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, PreviousLightOpticalDepthTexture6)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, PreviousLightOpticalDepthTexture7)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<uint>, RenderOccupancyTexture0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<uint>, RenderOccupancyTexture1)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<uint>, RenderOccupancyTexture2)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<uint>, RenderOccupancyTexture3)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<uint>, RenderOccupancyTexture4)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<uint>, RenderOccupancyTexture5)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<uint>, RenderOccupancyTexture6)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<uint>, RenderOccupancyTexture7)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, DetailNoiseTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorSampler)
		SHADER_PARAMETER_SAMPLER(SamplerState, VolumeSampler)
		SHADER_PARAMETER_SAMPLER(SamplerState, DetailNoiseSampler)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeTemporalResolvePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeTemporalResolvePS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeTemporalResolvePS, FGlobalShader);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FVector4f, SceneDepthPixelScaleBias)
		SHADER_PARAMETER(FIntRect, ViewRect)
		SHADER_PARAMETER(FIntRect, SceneDepthViewRect)
		SHADER_PARAMETER(FVector2f, CurrentSmokeSize)
		SHADER_PARAMETER(float, BilateralDepthSensitivity)
		SHADER_PARAMETER(float, HistoryWeight)
		SHADER_PARAMETER(float, TransmittanceRejection)
		SHADER_PARAMETER(float, NeighborhoodClampExpansion)
		SHADER_PARAMETER(uint32, bHistoryValid)
		SHADER_PARAMETER(FMatrix44f, InvViewProjection)
		SHADER_PARAMETER(FMatrix44f, PreviousViewProjection)
		SHADER_PARAMETER(FVector3f, CurrentViewOrigin)
		SHADER_PARAMETER(FVector3f, PreviousViewOrigin)
		SHADER_PARAMETER(float, DepthRejectionCm)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, CurrentSmokeTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, HistorySmokeTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, HistoryDepthTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, SceneDepthTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SmokeSampler)
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
