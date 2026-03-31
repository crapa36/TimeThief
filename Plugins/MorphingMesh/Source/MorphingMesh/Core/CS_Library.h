// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "HLSLTypeAliases.h"

using namespace UE::HLSL;

BEGIN_UNIFORM_BUFFER_STRUCT(FConstBuffer,)
	SHADER_PARAMETER(uint3, GridSize)
	SHADER_PARAMETER(float, IsoLevel)
	SHADER_PARAMETER(float3, BoxMin)
	SHADER_PARAMETER(uint, NumVoxels)
	SHADER_PARAMETER(float3, VoxelSize)
	SHADER_PARAMETER(float, MC_Padding0)
	SHADER_PARAMETER(float3, Alpha)
	SHADER_PARAMETER(float, MC_Padding1)
END_UNIFORM_BUFFER_STRUCT()


class FClassify : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FClassify);
	SHADER_USE_PARAMETER_STRUCT(FClassify, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FConstBuffer, Constants)
		SHADER_PARAMETER_TEXTURE(Texture3D<float>, Density0)
		SHADER_PARAMETER_TEXTURE(Texture3D<float>, Density1)
		SHADER_PARAMETER_TEXTURE(Texture3D<float>, Density2)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, TriCount)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, CubeCase)
	END_SHADER_PARAMETER_STRUCT()
};

class FBlockScan : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FBlockScan);
	SHADER_USE_PARAMETER_STRUCT(FBlockScan, FGlobalShader);
	
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, InputBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, BlockScanBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, OffsetBuffer)
	END_SHADER_PARAMETER_STRUCT()
};

class FAddOffset : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FAddOffset);
	SHADER_USE_PARAMETER_STRUCT(FAddOffset, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, OffsetBuffer)
		SHADER_PARAMETER(UE::HLSL::uint, OffsetBufferSize)
	END_SHADER_PARAMETER_STRUCT()
};

class FEmit : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FEmit);
	SHADER_USE_PARAMETER_STRUCT(FEmit, FGlobalShader);
	
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FConstBuffer, Constants)
		SHADER_PARAMETER_TEXTURE(Texture3D<float>, Density0)
		SHADER_PARAMETER_TEXTURE(Texture3D<float>, Density1)
		SHADER_PARAMETER_TEXTURE(Texture3D<float>, Density2)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CubeCase)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, TriCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, OffsetBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, PrefixBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float3>, PositionBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float3>, TangentsBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, IndirectArgsBuffer)
		SHADER_PARAMETER_TEXTURE(Texture3D, UVMap)
		SHADER_PARAMETER_SAMPLER(SamplerState, UVMapSampler)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float2>, UVBuffer)
	END_SHADER_PARAMETER_STRUCT()
};