// Fill out your copyright notice in the Description page of Project Settings.


#include "MorphingMeshSubsystem.h"
#include "MorphingMeshViewExtension.h"

void UMorphingMeshSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ViewExtension = FSceneViewExtensions::NewExtension<FMorphingMeshViewExtension>();
}

void UMorphingMeshSubsystem::Deinitialize()
{
	Super::Deinitialize();
	ViewExtension.Reset();
}
