// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MorphingMeshSubsystem.generated.h"

class FMorphingMeshViewExtension;
/**
 * 
 */
UCLASS()
class MORPHINGMESH_API UMorphingMeshSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	TSharedPtr<FMorphingMeshViewExtension, ESPMode::ThreadSafe> ViewExtension;
};
