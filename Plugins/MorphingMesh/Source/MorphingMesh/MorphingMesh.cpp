// Copyright Epic Games, Inc. All Rights Reserved.

#include "MorphingMesh.h"

#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "FMorphingMeshModule"

void FMorphingMeshModule::StartupModule()
{
	FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("MorphingMesh"))->GetBaseDir(), TEXT("Shaders"));
	if(!AllShaderSourceDirectoryMappings().Contains(TEXT("/MorphingMeshShaders")))
	{
		AddShaderSourceDirectoryMapping(TEXT("/MorphingMeshShaders"), PluginShaderDir);
	}
}

void FMorphingMeshModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FMorphingMeshModule, MorphingMesh)