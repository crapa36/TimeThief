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
};

struct FTimeThiefSmokeCarrierParticleShaderData
{
	FVector4f LocalPositionRadius = FVector4f::Zero();
	FVector4f VelocityPhase = FVector4f::Zero();
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

class FTimeThiefSmokeBulletSuppressCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeBulletSuppressCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeBulletSuppressCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		TIME_THIEF_SMOKE_BULLET_SUPPRESS_CS_PARAMETERS
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

class FTimeThiefSmokeDivergenceCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeDivergenceCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeDivergenceCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		TIME_THIEF_SMOKE_DIVERGENCE_CS_PARAMETERS
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

class FTimeThiefSmokeProjectVelocityCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeProjectVelocityCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeProjectVelocityCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		TIME_THIEF_SMOKE_PROJECT_VELOCITY_CS_PARAMETERS
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
