// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "PrimitiveSceneProxy.h"
#include "MarchingCubesRenderResource.h"

class ULiquidMeshComponent;
// FStaticMeshSceneProxy;
class MORPHINGMESH_API FLiquidMeshProxy : public FPrimitiveSceneProxy
{
public:
	FMaterialRelevance MaterialRelevance;
	FMaterialRenderProxy* MaterialRenderProxy = nullptr;
	
	TUniquePtr<FMarchingCubesRenderResource> RenderResource = {nullptr};
	
public:
	FLiquidMeshProxy(const ULiquidMeshComponent* InComponent);
	virtual ~FLiquidMeshProxy() override;
	
	void UpdateRenderResource(FRDGBuilder& GraphicBuilder, 
		const FBox& InBound,
		const FVector3f& Alpha,
		const TArray<TObjectPtr<UVolumeTexture>>& VolumeTextures);
	
	virtual void CreateRenderThreadResources(FRHICommandListBase& RHICmdList) override;
	virtual void DestroyRenderThreadResources() override;
	
	virtual void GetDynamicMeshElements(
		const TArray<const FSceneView*>& Views, 
		const FSceneViewFamily& ViewFamily, 
		uint32 VisibilityMap, 
		FMeshElementCollector& Collector) const override;
	
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	
	virtual SIZE_T GetTypeHash() const override
	{
		static size_t Unique;
		return reinterpret_cast<SIZE_T>(&Unique);
	}
	
	virtual uint32 GetMemoryFootprint() const override
	{
		return sizeof(*this) + GetAllocatedSize();
	}
};
