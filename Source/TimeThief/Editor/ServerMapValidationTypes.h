#pragma once

#include "CoreMinimal.h"
#include "ServerMapTags.h"

struct FServerMapValidationItem
{
	FString ActorName;
	FString Reason;
	FString StaticMeshName;
	FString PresetName;
};

struct FServerMapValidationReport
{
	int32 TaggedActorCount = 0;

	int32 ActorsWithValidShapes = 0;
	int32 ActorsUsingPreset = 0;
	int32 ActorsMissingStaticMesh = 0;
	int32 ActorsMissingPreset = 0;
	int32 ActorsWithNullStaticMesh = 0;

	int32 ActorsGeneratedFromSimple = 0;
	int32 ActorsGeneratedFromConvexFallback = 0;
	int32 ActorsGeneratedFromBoundsFallback = 0;

	TArray<FServerMapValidationItem> Items;
};