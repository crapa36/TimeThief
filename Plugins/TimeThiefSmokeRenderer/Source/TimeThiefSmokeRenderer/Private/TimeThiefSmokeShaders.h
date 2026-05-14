#pragma once

#include "GlobalShader.h"
#include "SceneView.h"
#include "ShaderParameterStruct.h"

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
		SHADER_PARAMETER(FIntVector, GridResolution)
		SHADER_PARAMETER(FVector3f, BoundsExtent)
		SHADER_PARAMETER(float, InitialDensity)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float>, OutDensity)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, OutVelocity)
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeApplyEventsCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeApplyEventsCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeApplyEventsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FIntVector, GridResolution)
		SHADER_PARAMETER(FVector3f, BoundsExtent)
		SHADER_PARAMETER(float, DeltaSeconds)
		SHADER_PARAMETER(int32, EventCount)
		SHADER_PARAMETER(FMatrix44f, LocalToWorld)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FTimeThiefSmokeEventShaderData>, Events)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, DensityIn)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, VelocityIn)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, ObstacleTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, VolumeSampler)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float>, OutDensity)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, OutVelocity)
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeBulletSuppressCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeBulletSuppressCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeBulletSuppressCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FIntVector, GridResolution)
		SHADER_PARAMETER(FVector3f, BoundsExtent)
		SHADER_PARAMETER(float, DeltaSeconds)
		SHADER_PARAMETER(float, SuppressionLife)
		SHADER_PARAMETER(int32, EventCount)
		SHADER_PARAMETER(FMatrix44f, LocalToWorld)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FTimeThiefSmokeEventShaderData>, Events)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, SuppressionIn)
		SHADER_PARAMETER_SAMPLER(SamplerState, VolumeSampler)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float>, OutSuppression)
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeDynamicObstacleCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeDynamicObstacleCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeDynamicObstacleCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FIntVector, GridResolution)
		SHADER_PARAMETER(FVector3f, BoundsExtent)
		SHADER_PARAMETER(int32, EventCount)
		SHADER_PARAMETER(FMatrix44f, LocalToWorld)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FTimeThiefSmokeEventShaderData>, Events)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, DensityIn)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, VelocityIn)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, ObstacleTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, VolumeSampler)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float>, OutDensity)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, OutVelocity)
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeCarrierUpdateCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeCarrierUpdateCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeCarrierUpdateCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(float, DeltaSeconds)
		SHADER_PARAMETER(float, DriftSpeed)
		SHADER_PARAMETER(float, InteractionStrength)
		SHADER_PARAMETER(int32, CarrierParticleCount)
		SHADER_PARAMETER(int32, EventCount)
		SHADER_PARAMETER(FVector3f, BoundsExtent)
		SHADER_PARAMETER(float, ObstacleTexelSize)
		SHADER_PARAMETER(FMatrix44f, LocalToWorld)
		SHADER_PARAMETER(FMatrix44f, WorldToLocal)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FTimeThiefSmokeCarrierParticleShaderData>, CarrierParticlesIn)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FTimeThiefSmokeEventShaderData>, Events)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, ObstacleTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, VolumeSampler)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FTimeThiefSmokeCarrierParticleShaderData>, OutCarrierParticles)
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeSimulateCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeSimulateCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeSimulateCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FIntVector, GridResolution)
		SHADER_PARAMETER(FVector3f, BoundsExtent)
		SHADER_PARAMETER(float, DeltaSeconds)
		SHADER_PARAMETER(float, InitialDensity)
		SHADER_PARAMETER(float, AgeSeconds)
		SHADER_PARAMETER(float, DurationSeconds)
		SHADER_PARAMETER(float, SmokeFadeOutDuration)
		SHADER_PARAMETER(float, PlumeEmissionDuration)
		SHADER_PARAMETER(float, PlumeSourceRadius)
		SHADER_PARAMETER(float, PlumeExpansionVelocity)
		SHADER_PARAMETER(float, PlumeRiseVelocity)
		SHADER_PARAMETER(float, DensityDissipation)
		SHADER_PARAMETER(float, VelocityDamping)
		SHADER_PARAMETER(float, VorticityStrength)
		SHADER_PARAMETER(uint32, bUseMacCormackAdvection)
		SHADER_PARAMETER(int32, CarrierParticleCount)
		SHADER_PARAMETER(FMatrix44f, LocalToWorld)
		SHADER_PARAMETER(FMatrix44f, WorldToLocal)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FTimeThiefSmokeCarrierParticleShaderData>, CarrierParticles)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, DensityIn)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, VelocityIn)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, ObstacleTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, BulletSuppressionTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, VolumeSampler)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float>, OutDensity)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, OutVelocity)
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeDivergenceCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeDivergenceCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeDivergenceCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FIntVector, GridResolution)
		SHADER_PARAMETER(FVector3f, CellSize)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, VelocityIn)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, ObstacleTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, VolumeSampler)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float>, OutDivergence)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float>, OutPressure)
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokePressureJacobiCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokePressureJacobiCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokePressureJacobiCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FIntVector, GridResolution)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, PressureIn)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, DivergenceIn)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float>, OutPressure)
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeProjectVelocityCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeProjectVelocityCS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeProjectVelocityCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FIntVector, GridResolution)
		SHADER_PARAMETER(FVector3f, CellSize)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, VelocityIn)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, PressureIn)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, ObstacleTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, VolumeSampler)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, OutVelocity)
	END_SHADER_PARAMETER_STRUCT()
};

class FTimeThiefSmokeCompositePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTimeThiefSmokeCompositePS);
	SHADER_USE_PARAMETER_STRUCT(FTimeThiefSmokeCompositePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER(FVector4f, SceneColorUVScaleBias)
		SHADER_PARAMETER(FIntRect, ViewRect)
		SHADER_PARAMETER(FVector3f, BoundsExtent)
		SHADER_PARAMETER(float, Extinction)
		SHADER_PARAMETER(float, ScatteringAlbedo)
		SHADER_PARAMETER(float, ScatteringAnisotropy)
		SHADER_PARAMETER(float, AgeSeconds)
		SHADER_PARAMETER(float, DurationSeconds)
		SHADER_PARAMETER(float, SmokeFadeOutDuration)
		SHADER_PARAMETER(int32, RenderStepCount)
		SHADER_PARAMETER(int32, DebugMode)
		SHADER_PARAMETER(int32, CarrierParticleCount)
		SHADER_PARAMETER(int32, EventCount)
		SHADER_PARAMETER(FMatrix44f, InvViewProjection)
		SHADER_PARAMETER(FMatrix44f, LocalToWorld)
		SHADER_PARAMETER(FMatrix44f, WorldToLocal)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FTimeThiefSmokeCarrierParticleShaderData>, CarrierParticles)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FTimeThiefSmokeEventShaderData>, Events)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneDepthTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, DensityTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, ObstacleTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, BulletSuppressionTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorSampler)
		SHADER_PARAMETER_SAMPLER(SamplerState, VolumeSampler)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};
