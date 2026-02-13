// Fill out your copyright notice in the Description page of Project Settings.


#include "LiquidMeshProxy.h"

#include "MarchingCubesRenderResource.h"
#include "LiquidMeshComponent.h"
#include "RenderGraphBuilder.h"
#include "../Settings.h"
#include "../MorphingMeshComponent.h"

FLiquidMeshProxy::FLiquidMeshProxy(const ULiquidMeshComponent* InComponent)
	: FPrimitiveSceneProxy{InComponent}
{
	MaterialRelevance = UMaterial::GetDefaultMaterial(MD_Surface)->GetRelevance_Concurrent(
		GetScene().GetShaderPlatform());
	MaterialRenderProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();

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


	if (InComponent->ParentComponent)
	{
		if (InComponent->ParentComponent->LiquidMaterial)
		{
			MaterialRelevance = InComponent->ParentComponent->LiquidMaterial->GetRelevance_Concurrent(
				GetScene().GetShaderPlatform());
			MaterialRenderProxy = InComponent->ParentComponent->LiquidMaterial->GetRenderProxy();
			bVerifyUsedMaterials = false;
		}
	}
}

FLiquidMeshProxy::~FLiquidMeshProxy()
{
	RenderResource->ReleaseResource();
}

void FLiquidMeshProxy::UpdateRenderResource(FRDGBuilder& GraphicBuilder,
                                            const FBox& InBound,
                                            const FVector3f& Alpha,
                                            const TArray<TObjectPtr<UVolumeTexture>>& VolumeTextures)
{
	if (RenderResource)
	{
		RenderResource->RunComputeShader(GraphicBuilder, InBound, Alpha, VolumeTextures);
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
	if (!RenderResource || !RenderResource->IsReady())
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
