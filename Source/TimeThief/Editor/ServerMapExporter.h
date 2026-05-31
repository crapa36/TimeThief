#pragma once

#include "CoreMinimal.h"
#include "ServerMapTags.h"
#include "ServerMapExportTypes.h"
#include "ServerMapValidationTypes.h"

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


class ServerMapExporter
{
public:
	static bool ExportActorsWithTagToFile(UWorld* World, const FName& RequiredTag, const FString& OutputPath);
	static bool ExportSpawnLocationsToJsonFile(UWorld* World, const FName& StoreTag, const FName& ChestTag, const FString& OutputPath, bool bDisableExportedActors);
	static bool ExportMonsterSpawnLocationsToJsonFile(UWorld* World, const FString& OutputPath, bool bDisableExportedActors);
	static bool ExportPresetToFile(AActor* Actor, UServerCollisionPresetDataAsset* PresetAsset, const FString& OutputPath);
	static bool ExportSelectedActorResolvedToFile(const FString& OutputPath);

	static bool GenerateShapesFromSelectedActor(bool bClearExistingGeneratedShapes = true);
	static bool ClearGeneratedShapesFromSelectedActor();
	static bool SaveSelectedActorShapesToPresets(const FString& PresetFolderPath, bool bOnlyGeneratedShapes);
	static bool ValidateSelectedActor(FServerMapValidationReport& OutReport);

	static bool GenerateBoxFromActorStaticMesh(AActor* Actor, bool bClearExistingGeneratedShapes = true);
	static bool GenerateShapesFromActorStaticMesh(AActor* Actor, bool bClearExistingGeneratedShapes = true);
	static int32 GenerateShapesForActorsWithTag(UWorld* World, const FName& RequiredTag, bool bSkipActorsWithExistingShapes, bool bSkipActorsWithExistingPreset, bool bClearExistingGeneratedShapesBeforeRegenerate);
	static int32 ClearGeneratedShapesForActorsWithTag(UWorld* World, const FName& RequiredTag);
	static int32 SaveGeneratedShapesToPresetsForActorsWithTag(UWorld* World, const FName& RequiredTag, const FString& PresetFolderPath, bool bOnlySaveGeneratedShapes);
	static bool ValidateActorsWithTag(UWorld* World, const FName& RequiredTag, FServerMapValidationReport& OutReport);
	
	static bool SpawnPresetShapesForSelectedActor(bool bClearExistingPresetShapes = true);
	static int32 SpawnPresetShapesForActor(AActor* Actor, bool bClearExistingPresetShapes);
	static int32 SpawnPresetShapesForStaticMeshComponent(AActor* Actor, UStaticMeshComponent* StaticMeshComponent, const UServerCollisionPresetDataAsset* PresetAsset);
	
	static bool ApproveSelectedActorGeneratedShapes();
	
	static UShapeComponent* CreateShapeComponentFromPresetCollider(AActor* OwnerActor, USceneComponent* AttachParent, const FServerCollisionPresetCollider& PresetCollider);
	static int32 SpawnPresetShapesForActorsWithTag(UWorld* World, const FName& RequiredTag, bool bClearExistingPresetShapes);
	
	static void AddCollisionTagToActors();
	static int32 DisableActorsWithTag(UWorld* World, const FName& RequiredTag);
	
// 맵 데이터 정리용
	
	static void CheckSelectedActorsStaticMeshActor();
	
	static int32 ReplaceSelectedActorWithStaticMeshActors(UWorld* World, const FName& AddTag);
	
private:
	static AActor* GetFirstSelectedActor();
	static UStaticMeshComponent* FindFirstStaticMeshComponent(AActor* Actor);
	
	static void CollectActorsWithTag(UWorld* World, const FName& RequiredTag, TArray<AActor*>& OutActors);
	static void CollectStaticMeshComponents(AActor* Actor, TArray<UStaticMeshComponent*>& OutComponents);
	static void CollectShapeComponents(AActor* Actor, TArray<UShapeComponent*>& OutShapeComponents);
	static void CollectAuthoringShapeComponents(AActor* Actor, bool bOnlyGeneratedShapes, TArray<UShapeComponent*>& OutShapeComponents);
	static void CollectAuthoringShapeComponentsForStaticMeshComponent(AActor* Actor, UStaticMeshComponent* StaticMeshComponent, bool bOnlyGeneratedShapes, TArray<UShapeComponent*>& OutShapeComponents);
	
	static int32 GenerateShapesFromActorStaticMeshComponents(AActor* Actor, bool bClearExistingGeneratedShapes);
	static int32 SaveActorShapesToPresets(AActor* Actor, const FString& PresetFolderPath, bool bOnlyGeneratedShapes);
	static bool SaveActorShapesToPresetForStaticMeshComponent(AActor* Actor, UStaticMeshComponent* StaticMeshComponent, UServerCollisionPresetDataAsset* PresetAsset, bool bOnlyGeneratedShapes);
	static bool ValidateActor(AActor* Actor, FServerMapValidationReport& InOutReport);
	
	static void RemoveGeneratedShapeComponents(AActor* Actor);
	static int32 CountGeneratedShapeComponents(AActor* Actor);
	static bool GenerateShapesFromStaticMeshComponent(UStaticMeshComponent* StaticMeshComponent, AActor* OwnerActor);
	static int32 GeneratePrimitiveShapesFromBodySetup(UStaticMeshComponent* StaticMeshComponent, AActor* OwnerActor);
	static int32 GenerateConvexFallbackShapes(UStaticMeshComponent* StaticMeshComponent, AActor* OwnerActor);
	
	static UBoxComponent* CreateGeneratedBoxComponent(AActor* OwnerActor, USceneComponent* AttachParent, const FVector& RelativeLocation, const FRotator& RelativeRotation, const FVector& BoxExtent);
	static USphereComponent* CreateGeneratedSphereComponent(AActor* OwnerActor, USceneComponent* AttachParent, const FVector& RelativeLocation, float Radius);
	static UCapsuleComponent* CreateGeneratedCapsuleComponent(AActor* OwnerActor, USceneComponent* AttachParent, const FVector& RelativeLocation, const FRotator& RelativeRotation, float Radius, float HalfHeight);
	static UBoxComponent* CreateGeneratedBoundsBoxComponent(UStaticMeshComponent* StaticMeshComponent, AActor* OwnerActor);
	
	static void AppendValidationItem(FServerMapValidationReport& Report, AActor* Actor, const FString& Reason, UStaticMesh* StaticMesh = nullptr, UServerCollisionPresetDataAsset* PresetAsset = nullptr);
	static void AccumulateGeneratedSourceStats(AActor* Actor, FServerMapValidationReport& Report);
	static void LogValidationReport(const FName& RequiredTag, const FServerMapValidationReport& Report);
	static bool HasValidShapeComponent(AActor* Actor);
	static bool ShouldExportShapeComponent(const UShapeComponent* ShapeComponent);
	static bool IsValidShapeComponentForExport(const UShapeComponent* ShapeComponent);
	
	static UServerCollisionPresetDataAsset* FindPresetForStaticMesh(UStaticMesh* StaticMesh);
	static UServerCollisionPresetDataAsset* FindOrCreatePresetForStaticMesh(UStaticMesh* StaticMesh, const FString& PresetFolderPath);
	static void CollectGeneratedShapeComponents(AActor* Actor, TArray<UShapeComponent*>& OutShapeComponents);
	static bool BuildPresetColliderFromShapeComponent(const UShapeComponent* ShapeComponent, FServerCollisionPresetCollider& OutPresetCollider);
	static bool BuildPresetColliderFromBoxComponent(const UBoxComponent* BoxComponent, FServerCollisionPresetCollider& OutPresetCollider);
	static bool BuildPresetColliderFromSphereComponent(const USphereComponent* SphereComponent, FServerCollisionPresetCollider& OutPresetCollider);
	static bool BuildPresetColliderFromCapsuleComponent(const UCapsuleComponent* CapsuleComponent, FServerCollisionPresetCollider& OutPresetCollider);
	static int32 BuildColliderDataListFromPreset(AActor* Actor, UStaticMeshComponent* StaticMeshComponent, const UServerCollisionPresetDataAsset* PresetAsset, TArray<se::map::ColliderData>& OutColliders, TArray<FServerMapColliderDebugRecord>& OutDebugRecords, FServerMapExportStats& Summary);
	static bool BuildWorldColliderDataFromPresetCollider(const FServerCollisionPresetCollider& PresetCollider, const FTransform& MeshComponentWorldTransform, se::map::ColliderData& OutColliderData);

	static int32 BuildColliderDataListFromActorResolved(AActor* Actor, TArray<se::map::ColliderData>& OutColliders, TArray<FServerMapColliderDebugRecord>& OutDebugRecords,	FServerMapExportStats& Summary);
	static int32 BuildColliderDataListFromActor(AActor* Actor, TArray<se::map::ColliderData>& OutColliders, TArray<FServerMapColliderDebugRecord>& OutDebugRecords, FServerMapExportStats& Summary);
	static bool BuildColliderDataFromShapeComponent(const UShapeComponent* ShapeComponent, se::map::ColliderData& OutColliderData);
	static bool BuildColliderDataFromBoxComponent(const UBoxComponent* BoxComponent, se::map::ColliderData& OutColliderData);
	static bool BuildColliderDataFromSphereComponent(const USphereComponent* SphereComponent, se::map::ColliderData& OutColliderData);
	static bool BuildColliderDataFromCapsuleComponent(const UCapsuleComponent* CapsuleComponent, se::map::ColliderData& OutColliderData);
	static uint32 BuildColliderFlagsFromShapeComponent(const UShapeComponent* ShapeComponent);
	
	static void AppendDebugRecord(const AActor* Actor, const UActorComponent* Component, const se::map::ColliderData& ColliderData, TArray<FServerMapColliderDebugRecord>& OutDebugRecords);
	static void AccumulateSummary(const se::map::ColliderData& ColliderData, FServerMapExportStats& Summary);
	static void LogExportSummary(const FName& RequiredTag, const FString& OutputPath, const FServerMapExportStats& Summary);
	
	static bool WriteServerMapFile(const FString& OutputPath, const se::map::MapHeader& MapHeader, const TArray<se::map::ColliderData>& Colliders);
	static bool WriteDebugJsonFile(const FString& OutputPath, const TArray<FServerMapColliderDebugRecord>& DebugRecords);
	static FString MakeDebugJsonOutputPath(const FString& BinaryOutputPath);
	
	static void GetSelectedActors(TArray<AActor*>& OutActors);

};
