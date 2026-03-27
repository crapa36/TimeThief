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
class UServerCollisionPresetDataAsset;
struct FServerCollisionPresetCollider;

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
	
	int32 ShapeSourceActorCount = 0;
	int32 PresetSourceActorCount = 0;
	int32 MissingPresetActorCount = 0;
};

struct FServerMapDebugColliderRecord
{
	FString ActorName;
	FString ComponentName;

	se::map::ColliderData ColliderData;
};

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

class ServerMapExporter
{
public:
	static bool ExportSelectedActorBoxesToFile(const FString& OutputPath);
	static bool ExportActorsWithTagToFile(UWorld* World, const FName& RequiredTag, const FString& OutputPath);
	static bool ExportPresetToFile(AActor* Actor, UServerCollisionPresetDataAsset* PresetAsset, const FString& OutputPath);

	static bool GenerateBoxFromSelectedStaticMesh();
	static bool GenerateBoxFromActorStaticMesh(AActor* Actor, bool bClearExistingGeneratedShapes = true);

	static bool GenerateShapesFromSelectedStaticMesh();
	static bool GenerateShapesFromActorStaticMesh(AActor* Actor, bool bClearExistingGeneratedShapes = true);

	static int32 GenerateShapesForActorsWithTag(UWorld* World, const FName& RequiredTag, bool bSkipActorsWithExistingShapes, bool bSkipActorsWithExistingPreset, bool bClearExistingGeneratedShapesBeforeRegenerate);
	static int32 ClearGeneratedShapesForActorsWithTag(UWorld* World, const FName& RequiredTag);
	static int32 SaveGeneratedShapesToPresetsForActorsWithTag(UWorld* World, const FName& RequiredTag, const FString& PresetFolderPath, bool bOnlySaveGeneratedShapes);
	static bool ValidateActorsWithTag(UWorld* World, const FName& RequiredTag, FServerMapValidationReport& OutReport);

	static bool GenerateShapesFromStaticMeshComponent(UStaticMeshComponent* StaticMeshComponent, AActor* OwnerActor);
	static int32 GeneratePrimitiveShapesFromBodySetup(UStaticMeshComponent* StaticMeshComponent, AActor* OwnerActor);
	static int32 GenerateConvexFallbackShapes(UStaticMeshComponent* StaticMeshComponent, AActor* OwnerActor);

	static UBoxComponent* CreateGeneratedBoxComponent(AActor* OwnerActor, USceneComponent* AttachParent, const FVector& RelativeLocation, const FRotator& RelativeRotation, const FVector& BoxExtent);
	static USphereComponent* CreateGeneratedSphereComponent(AActor* OwnerActor, USceneComponent* AttachParent, const FVector& RelativeLocation, float Radius);
	static UCapsuleComponent* CreateGeneratedCapsuleComponent(AActor* OwnerActor, USceneComponent* AttachParent, const FVector& RelativeLocation, const FRotator& RelativeRotation, float Radius, float HalfHeight);
	static UBoxComponent* CreateGeneratedBoundsBoxComponent(UStaticMeshComponent* StaticMeshComponent, AActor* OwnerActor);
	
	static bool SaveSelectedActorGeneratedShapesToPreset(UServerCollisionPresetDataAsset* PresetAsset, bool bOnlyGeneratedShapes = true);

private:
	static AActor* GetFirstSelectedActor();

	static UServerCollisionPresetDataAsset* FindPresetForStaticMesh(UStaticMesh* StaticMesh);
	static UServerCollisionPresetDataAsset* FindOrCreatePresetForStaticMesh(UStaticMesh* StaticMesh, const FString& PresetFolderPath);

	static UStaticMeshComponent* FindFirstStaticMeshComponent(AActor* Actor);
	static void RemoveGeneratedShapeComponents(AActor* Actor);
	static int32 CountGeneratedShapeComponents(AActor* Actor);

	static void CollectActorsWithTag(UWorld* World, const FName& RequiredTag, TArray<AActor*>& OutActors);
	static void CollectShapeComponents(AActor* Actor, TArray<UShapeComponent*>& OutShapeComponents);
	static void CollectAuthoringShapeComponents(AActor* Actor, bool bOnlyGeneratedShapes, TArray<UShapeComponent*>& OutShapeComponents);

	static bool ShouldExportShapeComponent(const UShapeComponent* ShapeComponent);
	static bool IsValidShapeComponentForExport(const UShapeComponent* ShapeComponent);
	static bool HasValidShapeComponent(AActor* Actor);

	static void AppendValidationItem(FServerMapValidationReport& Report, AActor* Actor, const FString& Reason, UStaticMesh* StaticMesh = nullptr, UServerCollisionPresetDataAsset* PresetAsset = nullptr);
	static void LogValidationReport(const FName& RequiredTag, const FServerMapValidationReport& Report);

	static int32 BuildColliderDataListFromActorResolved(AActor* Actor, TArray<se::map::ColliderData>& OutColliders, TArray<FServerMapDebugColliderRecord>& OutDebugRecords,	FServerMapExportSummary& Summary);

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

	static bool SaveActorGeneratedShapesToPreset(AActor* Actor, UServerCollisionPresetDataAsset* PresetAsset, bool bOnlyGeneratedShapes = true);
	static void CollectGeneratedShapeComponents(AActor* Actor, TArray<UShapeComponent*>& OutShapeComponents);

	static bool BuildPresetColliderFromShapeComponent(const UShapeComponent* ShapeComponent, FServerCollisionPresetCollider& OutPresetCollider);
	static bool BuildPresetColliderFromBoxComponent(const UBoxComponent* BoxComponent, FServerCollisionPresetCollider& OutPresetCollider);
	static bool BuildPresetColliderFromSphereComponent(const USphereComponent* SphereComponent, FServerCollisionPresetCollider& OutPresetCollider);
	static bool BuildPresetColliderFromCapsuleComponent(const UCapsuleComponent* CapsuleComponent, FServerCollisionPresetCollider& OutPresetCollider);

	static int32 BuildColliderDataListFromPreset(AActor* Actor, UStaticMeshComponent* StaticMeshComponent, const UServerCollisionPresetDataAsset* PresetAsset, TArray<se::map::ColliderData>& OutColliders, TArray<FServerMapDebugColliderRecord>& OutDebugRecords, FServerMapExportSummary& Summary);
	static bool BuildWorldColliderDataFromPresetCollider(const FServerCollisionPresetCollider& PresetCollider, const FTransform& MeshComponentWorldTransform, se::map::ColliderData& OutColliderData);
	
	static void AccumulateGeneratedSourceStats(AActor* Actor, FServerMapValidationReport& Report);

};
