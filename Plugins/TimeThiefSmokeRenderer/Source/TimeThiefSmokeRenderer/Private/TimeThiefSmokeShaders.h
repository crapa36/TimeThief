#pragma once

#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "TimeThiefSmokeShaderParameterMacros.h"

struct FTimeThiefSmokeEventShaderData
{
	FVector4f PositionRadius = FVector4f::Zero();
	FVector4f DirectionLength = FVector4f::Zero();
	FVector4f ExtentsStrength = FVector4f::Zero();
	FVector4f Rotation = FVector4f::Zero();
	FVector4f TypeShapeAgeSeed = FVector4f::Zero();
	FVector4f PreviousPositionSpeed = FVector4f::Zero();
};

struct FTimeThiefSmokeCarrierParticleShaderData
{
	FVector4f LocalPositionRadius = FVector4f::Zero();
	FVector4f VelocityPhase = FVector4f::Zero();
};

struct FTimeThiefSmokeVortexParticleShaderData
{
	FVector4f LocalPositionLife = FVector4f::Zero();
	FVector4f VelocityStrength = FVector4f::Zero();
	FVector4f AxisSeed = FVector4f(0.0f, 0.0f, 1.0f, 0.0f);
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

class FTimeThiefSmokeCarrierUpdateCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeCarrierUpdateCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeCarrierUpdateCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		TIME_THIEF_SMOKE_CARRIER_UPDATE_CS_PARAMETERS
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeBuildCarrierFieldCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeBuildCarrierFieldCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeBuildCarrierFieldCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		TIME_THIEF_SMOKE_BUILD_CARRIER_FIELD_CS_PARAMETERS
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
