#pragma once

#include "CoreMinimal.h"
#include "ServerMapTags.h"
#include "ServerMapExportTypes.h"

class AActor;
class UWorld;
class UStaticMeshComponent;
class USceneComponent;
class UBoxComponent;
class UShapeComponent;
class USphereComponent;
class UCapsuleComponent;

struct FServerMapExportSummary
{
	int32 TaggedActorCount = 0;
	int32 ExportedActorCount = 0;
	int32 ExportedColliderCount = 0;

	int32 BoxCount = 0;
	int32 SphereCount = 0;
	int32 CapsuleCount = 0;

	int32 IgnoredComponentCount = 0;
	int32 InvalidComponentCount = 0;
};

struct FServerMapDebugColliderRecord
{
	FString ActorName;
	FString ComponentName;

	se::map::ColliderData ColliderData;
};

class ServerMapExporter
{
public:
	static bool ExportSelectedActorBoxesToFile(const FString& OutputPath);
	static bool ExportActorsWithTagToFile(UWorld* World, const FName& RequiredTag, const FString& OutputPath);

	static bool GenerateBoxFromSelectedStaticMesh();
	static bool GenerateBoxFromActorStaticMesh(AActor* Actor, bool bClearExistingGeneratedShapes = true);
	
	static bool GenerateShapesFromSelectedStaticMesh();
	static bool GenerateShapesFromActorStaticMesh(AActor* Actor, bool bClearExistingGeneratedShapes = true);

	static bool GenerateShapesFromStaticMeshComponent(UStaticMeshComponent* StaticMeshComponent, AActor* OwnerActor);
	static int32 GeneratePrimitiveShapesFromBodySetup(UStaticMeshComponent* StaticMeshComponent, AActor* OwnerActor);

	static UBoxComponent* CreateGeneratedBoxComponent(AActor* OwnerActor, USceneComponent* AttachParent, const FVector& RelativeLocation, const FRotator& RelativeRotation, const FVector& BoxExtent);
	static USphereComponent* CreateGeneratedSphereComponent(AActor* OwnerActor, USceneComponent* AttachParent, const FVector& RelativeLocation, float Radius);
	static UCapsuleComponent* CreateGeneratedCapsuleComponent(AActor* OwnerActor, USceneComponent* AttachParent, const FVector& RelativeLocation, const FRotator& RelativeRotation, float Radius, float HalfHeight);
	static UBoxComponent* CreateGeneratedBoundsBoxComponent(UStaticMeshComponent* StaticMeshComponent, AActor* OwnerActor);
private:
	static AActor* GetFirstSelectedActor();

	static UStaticMeshComponent* FindFirstStaticMeshComponent(AActor* Actor);
	static void RemoveGeneratedShapeComponents(AActor* Actor);

	static void CollectActorsWithTag(UWorld* World, const FName& RequiredTag, TArray<AActor*>& OutActors);
	static void CollectShapeComponents(AActor* Actor, TArray<UShapeComponent*>& OutShapeComponents);

	static bool ShouldExportShapeComponent(const UShapeComponent* ShapeComponent);
	static bool IsValidShapeComponentForExport(const UShapeComponent* ShapeComponent);
	static bool HasValidShapeComponent(AActor* Actor);
	
	static uint32 BuildColliderFlagsFromShapeComponent(const UShapeComponent* ShapeComponent);

	static bool BuildColliderDataFromShapeComponent(const UShapeComponent* ShapeComponent, se::map::ColliderData& OutColliderData);
	static bool BuildColliderDataFromBoxComponent(const UBoxComponent* BoxComponent, se::map::ColliderData& OutColliderData);
	static bool BuildColliderDataFromSphereComponent(const USphereComponent* SphereComponent, se::map::ColliderData& OutColliderData);
	static bool BuildColliderDataFromCapsuleComponent(const UCapsuleComponent* CapsuleComponent, se::map::ColliderData& OutColliderData);

	static int32 BuildColliderDataListFromActor(AActor* Actor, TArray<se::map::ColliderData>& OutColliders, TArray<FServerMapDebugColliderRecord>& OutDebugRecords, FServerMapExportSummary& Summary);
	
	static void AccumulateSummary(const se::map::ColliderData& ColliderData, FServerMapExportSummary& Summary);
	static void LogExportSummary(const FName& RequiredTag, const FString& OutputPath, const FServerMapExportSummary& Summary);
	
	static bool WriteServerMapFile(const FString& OutputPath, const se::map::MapHeader& MapHeader, const TArray<se::map::ColliderData>& Colliders);
	static bool WriteDebugJsonFile(const FString& OutputPath, const TArray<FServerMapDebugColliderRecord>& DebugRecords);
	static FString MakeDebugJsonOutputPath(const FString& BinaryOutputPath);
	static void AppendDebugRecord(const AActor* Actor, const UActorComponent* Component, const se::map::ColliderData& ColliderData, TArray<FServerMapDebugColliderRecord>& OutDebugRecords);

};
