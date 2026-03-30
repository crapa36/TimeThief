// Fill out your copyright notice in the Description page of Project Settings.


#include "LiquidMeshProxy.h"

#include "MarchingCubesRenderResource.h"
#include "LiquidMeshComponent.h"
#include "RenderGraphBuilder.h"
#include "../Settings.h"
#include "../MorphingMeshComponent.h"
#include "Materials/MaterialRenderProxy.h"

FLiquidMeshProxy::FLiquidMeshProxy(const ULiquidMeshComponent* InComponent)
	: FPrimitiveSceneProxy{InComponent}
{
	RenderComponent = InComponent;
	UE_LOG(LogTemp, Warning, TEXT("FLiquidMeshProxy::FLiquidMeshProxy - RenderComponent: %s"),
	       *RenderComponent->GetName());
	if (InComponent->ParentComponent)
	{
		MaterialRelevance = InComponent->ParentComponent->LiquidMaterial->GetRelevance_Concurrent(
	GetScene().GetShaderPlatform());
		MaterialRenderProxy = InComponent->ParentComponent->LiquidMaterial->GetRenderProxy();
		bVerifyUsedMaterials = false;
	}
	else
	{
		MaterialRelevance = UMaterial::GetDefaultMaterial(MD_Surface)->GetRelevance_Concurrent(
		GetScene().GetShaderPlatform());
		MaterialRenderProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();	
	}

	bCastDynamicShadow = true;
	bCastStaticShadow = false;
	bCastDeepShadow = true;
	bEvaluateWorldPositionOffset = true;
	bHasWorldPositionOffsetVelocity = true;

	const int32 GridSize = InComponent->IsPlayerControlled()
		                       ? NumVoxelsTable[EVoxelResolution::High]
		                       : NumVoxelsTable[EVoxelResolution::Middle];

	RenderResource = MakeUnique<FMarchingCubesRenderResource>(InComponent->GetWorld()->GetFeatureLevel(),
	                                                          FIntVector{GridSize});
}

FLiquidMeshProxy::~FLiquidMeshProxy()
{
	RenderResource->ReleaseResource();
}

void FLiquidMeshProxy::CachingData()
{
	if (!RenderComponent)
	{
		return;
	}

	std::lock_guard Lock(CachingMutex);
	CachedBound = RenderComponent->GetBound();
	CachedAlpha = RenderComponent->GetAlpha();
	CachedDensityTextures = RenderComponent->GetDensityTextures();
	bRenderingEnable = RenderComponent->bRenderingEnable;
}

void FLiquidMeshProxy::UpdateRenderResource(FRDGBuilder& GraphicBuilder)
{
	if (!bRenderingEnable)
	{
		return;
	}
	if (RenderResource && RenderResource->IsReady())
	{
		FBox ParamBound;
		FVector3f ParamAlpha;
		TArray<TObjectPtr<UVolumeTexture>> ParamDensityTextures;
		{
			std::lock_guard Lock(CachingMutex);
			ParamBound = CachedBound;
			ParamAlpha = CachedAlpha;
			ParamDensityTextures = CachedDensityTextures;
		}
		RenderResource->RunComputeShader(GraphicBuilder, ParamBound, ParamAlpha, ParamDensityTextures);
	}
}

void FLiquidMeshProxy::CreateRenderThreadResources(FRHICommandListBase& RHICmdList)
{
	RenderResource->InitResource(RHICmdList);
}

void FLiquidMeshProxy::DestroyRenderThreadResources()
{
	RenderResource->ReleaseResource();
}

void FLiquidMeshProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views,
                                              const FSceneViewFamily& ViewFamily, uint32 VisibilityMap,
                                              FMeshElementCollector& Collector) const
{
	if (!RenderResource || !RenderResource->IsReady() || !bRenderingEnable || !MaterialRenderProxy)
	{
		return;
	}
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		if ((VisibilityMap & (1u << ViewIndex)) == 0)
		{
			continue;
		}
		FMeshBatch& Mesh = Collector.AllocateMesh();
		Mesh.VertexFactory = &RenderResource->VertexFactory;
		Mesh.MaterialRenderProxy = MaterialRenderProxy;
		Mesh.Type = PT_TriangleList;
		Mesh.DepthPriorityGroup = SDPG_World;
		Mesh.bCanApplyViewModeOverrides = true;
		Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
		Mesh.bUseForMaterial = true;
		Mesh.bUseForDepthPass = true;
		Mesh.CastShadow = true;
		Mesh.LODIndex = 0;
		Mesh.bUseAsOccluder = true;
		Mesh.CastRayTracedShadow = false;

		FMeshBatchElement& Elem = Mesh.Elements[0];
		Elem.PrimitiveUniformBuffer = GetUniformBuffer();
		Elem.IndirectArgsBuffer = RenderResource->IndirectArgsBuffer->GetRHI();
		Elem.IndirectArgsOffset = 0;
		Elem.NumPrimitives = 0;

		Collector.AddMesh(ViewIndex, Mesh);
	}
}

FPrimitiveViewRelevance FLiquidMeshProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Relevance;
	Relevance.bDrawRelevance = IsShown(View);
	Relevance.bShadowRelevance = true;
	Relevance.bDynamicRelevance = true;
	Relevance.bStaticRelevance = false;
	Relevance.bRenderInMainPass = true;
	Relevance.bRenderInDepthPass = true;
	Relevance.bVelocityRelevance = true;
	Relevance.bRenderInSecondStageDepthPass = true;

	MaterialRelevance.SetPrimitiveViewRelevance(Relevance);

	return Relevance;
}

void FLiquidMeshProxy::SetMaterial(UMaterialInterface* Material)
{
	if (Material)
	{
		MaterialRelevance = Material->GetRelevance_Concurrent(GetScene().GetShaderPlatform());
		MaterialRenderProxy = Material->GetRenderProxy();
		bVerifyUsedMaterials = false;
	}
}