#pragma once

#include "CoreMinimal.h"
#include "ServerMapExportTypes.h"

class AActor;
class UWorld;
class UBoxComponent;
class UShapeComponent;
class USphereComponent;
class UCapsuleComponent;

class ServerMapExporter
{
public:
	static bool ExportSelectedActorBoxesToFile(const FString& OutputPath);
	static bool ExportActorsWithTagToFile(UWorld* World, const FName& RequiredTag, const FString& OutputPath);

private:
	static AActor* GetFirstSelectedActor();

	static UBoxComponent* FindBoxComponent(AActor* Actor);
	static void CollectActorsWithTag(UWorld* World, const FName& RequiredTag, TArray<AActor*>& OutActors);
	static void CollectShapeComponents(AActor* Actor, TArray<UShapeComponent*>& OutShapeComponents);

	static uint32 BuildColliderFlagsFromShapeComponent(const UShapeComponent* ShapeComponent);

	static bool BuildColliderDataFromShapeComponent(const UShapeComponent* ShapeComponent, se::map::ColliderData& OutColliderData);
	static bool BuildColliderDataFromBoxComponent(const UBoxComponent* BoxComponent, se::map::ColliderData& OutColliderData);
	static bool BuildColliderDataFromSphereComponent(const USphereComponent* SphereComponent, se::map::ColliderData& OutColliderData);
	static bool BuildColliderDataFromCapsuleComponent(const UCapsuleComponent* CapsuleComponent, se::map::ColliderData& OutColliderData);

	static int32 BuildColliderDataListFromActor(AActor* Actor, TArray<se::map::ColliderData>& OutColliders);

	static bool WriteServerMapFile(const FString& OutputPath, const se::map::MapHeader& MapHeader, const TArray<se::map::ColliderData>& Colliders);
};
