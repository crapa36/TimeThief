// Fill out your copyright notice in the Description page of Project Settings.


#include "MorphingMeshViewExtension.h"
#include "LiquidMeshProxy.h"

FMorphingMeshViewExtension::FMorphingMeshViewExtension(const FAutoRegister& AutoRegister)
	:FSceneViewExtensionBase{AutoRegister}
{
}

void FMorphingMeshViewExtension::AddProxy(FLiquidMeshProxy* InProxy)
{
	if (!InProxy)
	{
		return;
	}

	ENQUEUE_RENDER_COMMAND(AddProxies)(
		[this, InProxy](FRHICommandListImmediate& RHICmdList)
		{
			ProxiesMutex.lock();
			Proxies.Add(InProxy);
			ProxiesMutex.unlock();
		});
}

void FMorphingMeshViewExtension::RemoveProxy(FLiquidMeshProxy* InProxy)
{
	if (!InProxy)
	{
		return;
	}


	ENQUEUE_RENDER_COMMAND(AddSceneProxy)(
		[this, InProxy](FRHICommandListImmediate& RHICmdList)
		{
			ProxiesMutex.lock();
			Proxies.RemoveSingleSwap(InProxy);
			ProxiesMutex.unlock();
		});
}

void FMorphingMeshViewExtension::PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder,
                                                                  FSceneViewFamily& InViewFamily)
{
	ProxiesMutex.lock();
	for (FLiquidMeshProxy* Proxy : Proxies)
	{
		Proxy->UpdateRenderResource(GraphBuilder);
	}
	ProxiesMutex.unlock();
}
