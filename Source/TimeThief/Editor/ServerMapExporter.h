#pragma once

#include "CoreMinimal.h"
#include "ServerMapExportTypes.h"

class AActor;
class UBoxComponent;

class ServerMapExporter
{
public:
	static bool ExportSelectedActorBoxToFile(const FString& OutputPath);
	
private:
	static AActor* GetFirstSelectedActor();
	static UBoxComponent* FindBoxComponent(AActor* Actor);
	static bool BuildColliderDataFromBoxComponent(const UBoxComponent* BoxComponent, se::map::ColliderData& ColliderData);
	static bool WriteServerMapFile(const FString& OutputPath, const se::map::MapHeader& MapHeader, const TArray<se::map::ColliderData>& Colliders);
	
};
