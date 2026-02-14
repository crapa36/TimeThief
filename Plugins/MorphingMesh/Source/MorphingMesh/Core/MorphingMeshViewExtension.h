// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SceneViewExtension.h"
#include <mutex>

class FLiquidMeshProxy;
/**
 * 
 */
class MORPHINGMESH_API FMorphingMeshViewExtension : public FSceneViewExtensionBase
{
	std::mutex ProxiesMutex;
public:
	FMorphingMeshViewExtension(const FAutoRegister& AutoRegister);
	
	void AddProxy(FLiquidMeshProxy* InProxy);
	void RemoveProxy(FLiquidMeshProxy* InProxy);
	
	virtual void PreRenderViewFamily_RenderThread(
		FRDGBuilder& GraphBuilder, 
		FSceneViewFamily& InViewFamily) override;
private:
	TArray<FLiquidMeshProxy*> Proxies;
};
