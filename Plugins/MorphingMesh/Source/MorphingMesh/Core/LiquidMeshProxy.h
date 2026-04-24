// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "PrimitiveSceneProxy.h"
#include "LiquidMeshComponent.h"
#include "MarchingCubesRenderResource.h"
#include <mutex>

class ULiquidMeshComponent;
// FStaticMeshSceneProxy;
class MORPHINGMESH_API FLiquidMeshProxy : public FPrimitiveSceneProxy
{
	const ULiquidMeshComponent* RenderComponent;
	
	mutable std::mutex CachingMutex;
	FBox CachedBound;
	FVector CachedAlpha;
	TArray<TObjectPtr<UVolumeTexture>> CachedDensityTextures;
	TArray<TObjectPtr<UVolumeTexture>> CachedUVMaps;
	TObjectPtr<UVolumeTexture> CachedBoneIndicesTexture;
	TArray<FMatrix44f> CachedBoneMatrices;
	bool bRenderingEnable = true;
	
public:
	FMaterialRelevance MaterialRelevance;
	FMaterialRenderProxy* MaterialRenderProxy = nullptr;
	
	TUniquePtr<FMarchingCubesRenderResource> RenderResource = {nullptr};

public:
	FLiquidMeshProxy(const ULiquidMeshComponent* InComponent);
	virtual ~FLiquidMeshProxy() override;
	
	void CachingData();
	void UpdateRenderResource(FRDGBuilder& GraphicBuilder);
	
	virtual void CreateRenderThreadResources(FRHICommandListBase& RHICmdList) override;
	virtual void DestroyRenderThreadResources() override;
	
	virtual void GetDynamicMeshElements(
		const TArray<const FSceneView*>& Views, 
		const FSceneViewFamily& ViewFamily, 
		uint32 VisibilityMap, 
		FMeshElementCollector& Collector) const override;
	
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	void SetMaterial(UMaterialInterface* Material);
	
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
