#pragma once

#include "CoreMinimal.h"
#include "ServerMapExportTypes.h"

class AActor;
class UBoxComponent;
class UWorld;

class ServerMapExporter
{
public:
	static bool ExportActorsWithTagToFile(UWorld* World, const FName& RequiredTag, const FString& OutputPath);
	
	static bool ExportSelectedActorBoxToFile(const FString& OutputPath);
	
private:
	static AActor* GetFirstSelectedActor();
	static UBoxComponent* FindBoxComponent(AActor* Actor);
	static bool BuildColliderDataFromBoxComponent(const UBoxComponent* BoxComponent, se::map::ColliderData& ColliderData);
	static bool WriteServerMapFile(const FString& OutputPath, const se::map::MapHeader& MapHeader, const TArray<se::map::ColliderData>& Colliders);
	
	static void CollectActorsWithTag(UWorld* World, const FName& RequiredTag, TArray<AActor*>& OutActors);
	static bool BuildColliderDataFromActor(AActor* Actor, se::map::ColliderData& OutColliderData);
	
};
