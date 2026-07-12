// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "LiquidMeshComponent.h"
#include "RenderResource.h"
#include "RHI.h"
/**
 * 
 */
class FMarchingCubesRenderResource : public FRenderResource
{
	class FVertexBufferWithRDG : public FVertexBuffer
	{
		public:
			TRefCountPtr<FRDGPooledBuffer> Buffer;
			FShaderResourceViewRHIRef ShaderResourceViewRHI;
			
			virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
			virtual void ReleaseRHI() override;
	};

public:
	TUniquePtr<FVertexBufferWithRDG> PositionBuffer;
	TUniquePtr<FVertexBufferWithRDG> TangentsBuffer;
	TUniquePtr<FVertexBufferWithRDG> UVBuffer;
	TRefCountPtr<FRDGPooledBuffer> IndirectArgsBuffer;
	
	FLocalVertexFactory VertexFactory;
	
	int32 NumVertex = 0;
	
	FMarchingCubesRenderResource(ERHIFeatureLevel::Type InFeatureLevel, const FIntVector3& InDimensions = FIntVector3::ZeroValue);
	
	void RunComputeShader(FRDGBuilder& GraphBuilder, 
	const FBox& InBound,
	const FVector& Alpha,
	const TArray<TObjectPtr<UVolumeTexture>>& VolumeTextures,
	const TArray<TObjectPtr<UVolumeTexture>>& UVMaps,
	TObjectPtr<UVolumeTexture> BoneIndicesTexture,
	TObjectPtr<UVolumeTexture> BoneWeightsTexture,
	const TArray<FMatrix44f>& SkinMatrices);
	
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
	virtual void ReleaseRHI() override;
	
	
	bool IsReady() const
	{
		return PositionBuffer.IsValid() && TangentsBuffer.IsValid() && IndirectArgsBuffer.IsValid() && VertexFactory.IsInitialized();
	}
};

