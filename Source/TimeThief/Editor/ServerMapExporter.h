#pragma once

#include "CoreMinimal.h"
#include "ServerMapExportTypes.h"

class AActor;
class UBoxComponent;
class UWorld;

class ServerMapExporter
{
public:
	static bool ExportSelectedActorBoxesToFile(const FString& OutputPath);
	static bool ExportActorsWithTagToFile(UWorld* World, const FName& RequiredTag, const FString& OutputPath);
	
private:
	static AActor* GetFirstSelectedActor();
	
	static UBoxComponent* FindBoxComponent(AActor* Actor);
	static void CollectBoxComponents(AActor* Actor, TArray<UBoxComponent*>& OutBoxComponents);
	
	static bool BuildColliderDataFromBoxComponent(const UBoxComponent* BoxComponent, se::map::ColliderData& ColliderData);
	static int32 BuildColliderDataListFromActor(AActor* Actor, TArray<se::map::ColliderData>& OutColliderData);
	static uint32 BuildColliderFlagsFromBoxComponent(const UBoxComponent* BoxComponent);
	
	static bool WriteServerMapFile(const FString& OutputPath, const se::map::MapHeader& MapHeader, const TArray<se::map::ColliderData>& Colliders);
	static void CollectActorsWithTag(UWorld* World, const FName& RequiredTag, TArray<AActor*>& OutActors);
	
};
