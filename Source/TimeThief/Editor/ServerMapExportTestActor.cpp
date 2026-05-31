#include "ServerMapExportTestActor.h"
#include "ServerMapExporter.h"


// Sets default values
AServerMapExportTestActor::AServerMapExportTestActor()
{
	PrimaryActorTick.bCanEverTick = false;
#if WITH_EDITORONLY_DATA
	bIsEditorOnlyActor = true;
#endif
}

void AServerMapExportTestActor::GenerateShapesForSelectedActor()
{
	const bool bResult = ServerMapExporter::GenerateShapesFromSelectedActor(
		bClearExistingGeneratedShapesBeforeRegenerate);

	UE_LOG(LogTemp, Log, TEXT("[ServerMapTest] GenerateShapesForSelectedActor result: %s"),
		bResult ? TEXT("true") : TEXT("false"));
}

void AServerMapExportTestActor::ClearGeneratedShapesForSelectedActor()
{
	const bool bResult = ServerMapExporter::ClearGeneratedShapesFromSelectedActor();

	UE_LOG(LogTemp, Log, TEXT("[ServerMapTest] ClearGeneratedShapesForSelectedActor result: %s"),
		bResult ? TEXT("true") : TEXT("false"));
}

void AServerMapExportTestActor::ValidateSelectedActor()
{
	FServerMapValidationReport Report;
	const bool bResult = ServerMapExporter::ValidateSelectedActor(Report);

	UE_LOG(LogTemp, Log, TEXT("[ServerMapTest] ValidateSelectedActor result: %s"),
		bResult ? TEXT("true") : TEXT("false"));
}

void AServerMapExportTestActor::ExportSelectedActorResolved()
{
	const FString OutputPath = FPaths::ProjectSavedDir() / TEXT("ServerMap/TestMap_SelectedResolved.servermap");
	const bool bResult = ServerMapExporter::ExportSelectedActorResolvedToFile(OutputPath);

	UE_LOG(LogTemp, Log, TEXT("[ServerMapTest] ExportSelectedActorResolved result: %s"),
		bResult ? TEXT("true") : TEXT("false"));
	UE_LOG(LogTemp, Log, TEXT("[ServerMapTest] OutputPath: %s"), *OutputPath);
}

void AServerMapExportTestActor::CheckSelectedActorsStaticMeshActor()
{
#if WITH_EDITOR
	ServerMapExporter::CheckSelectedActorsStaticMeshActor();
#endif
}

void AServerMapExportTestActor::LoadPresetShapesForSelectedActor()
{
	const bool bResult = ServerMapExporter::SpawnPresetShapesForSelectedActor(
		bClearExistingGeneratedShapesBeforeRegenerate);

	UE_LOG(LogTemp, Log, TEXT("[ServerMapTest] LoadPresetShapesForSelectedActor result: %s"),
		bResult ? TEXT("true") : TEXT("false"));
}

void AServerMapExportTestActor::SaveSelectedActorShapesToPresetAndClearWorldShapes()
{
	const bool bSaved = ServerMapExporter::SaveSelectedActorShapesToPresets(
		PresetFolderPath,
		false); // Generated + ManualApproved까지 저장하고 싶으면 false

	if (bSaved)
	{
		ServerMapExporter::ClearGeneratedShapesFromSelectedActor();
	}
}

void AServerMapExportTestActor::ApproveSelectedActorGeneratedShapes()
{
	ServerMapExporter::ApproveSelectedActorGeneratedShapes();
}

void AServerMapExportTestActor::GenerateShapesForTaggedActors()
{
	UWorld* World = GetWorld();
	const int32 GeneratedCount = ServerMapExporter::GenerateShapesForActorsWithTag(
		World,
		RequiredActorTag,
		bSkipActorsWithExistingShapes,
		bSkipActorsWithExistingPreset,
		bClearExistingGeneratedShapesBeforeRegenerate);

	UE_LOG(LogTemp, Log, TEXT("[ServerMapTest] GenerateShapesForTaggedActors generated actors: %d"), GeneratedCount);
}

void AServerMapExportTestActor::ClearGeneratedShapesForTaggedActors()
{
	UWorld* World = GetWorld();
	const int32 ClearedCount = ServerMapExporter::ClearGeneratedShapesForActorsWithTag(World, RequiredActorTag);

	UE_LOG(LogTemp, Log, TEXT("[ServerMapTest] ClearGeneratedShapesForTaggedActors cleared actors: %d"), ClearedCount);
}

void AServerMapExportTestActor::SaveTaggedGeneratedShapesToPresets()
{
	UWorld* World = GetWorld();
	const int32 SavedCount = ServerMapExporter::SaveGeneratedShapesToPresetsForActorsWithTag(
		World,
		RequiredActorTag,
		PresetFolderPath,
		bOnlySaveGeneratedShapes);

	UE_LOG(LogTemp, Log, TEXT("[ServerMapTest] SaveTaggedGeneratedShapesToPresets saved presets: %d"), SavedCount);
}

void AServerMapExportTestActor::ValidateTaggedActors()
{
	UWorld* World = GetWorld();
	FServerMapValidationReport Report;
	const bool bResult = ServerMapExporter::ValidateActorsWithTag(World, RequiredActorTag, Report);

	UE_LOG(LogTemp, Log, TEXT("[ServerMapTest] ValidateTaggedActors result: %s"),
		bResult ? TEXT("true") : TEXT("false"));
}

void AServerMapExportTestActor::ExportTaggedActorsResolved()
{
	UWorld* World = GetWorld();

	const FString OutputPath = FPaths::ProjectSavedDir() / TEXT("ServerMap/TestMap_Tagged.servermap");
	const bool bResult = ServerMapExporter::ExportActorsWithTagToFile(World, RequiredActorTag, OutputPath);

	UE_LOG(LogTemp, Log, TEXT("[ServerMapTest] ExportTaggedActorsResolved result: %s"), bResult ? TEXT("true") : TEXT("false"));
	UE_LOG(LogTemp, Log, TEXT("[ServerMapTest] OutputPath: %s"), *OutputPath);
}

void AServerMapExportTestActor::ExportStoreSpawnLocations()
{
	UWorld* World = GetWorld();

	const FString OutputPath = FPaths::ProjectSavedDir() / TEXT("ServerMap/TestStoreSpawnLocations.json");
	const bool bResult = ServerMapExporter::ExportSpawnLocationsToJsonFile(
		World,
		StoreActorTag,
		NAME_None,
		OutputPath,
		bDisableSpawnMarkerActorsAfterExport);

	UE_LOG(LogTemp, Log, TEXT("[ServerMapTest] ExportStoreSpawnLocations result: %s"), bResult ? TEXT("true") : TEXT("false"));
	UE_LOG(LogTemp, Log, TEXT("[ServerMapTest] OutputPath: %s"), *OutputPath);
}

void AServerMapExportTestActor::ExportChestSpawnLocations()
{
	UWorld* World = GetWorld();

	const FString OutputPath = FPaths::ProjectSavedDir() / TEXT("ServerMap/TestChestSpawnLocations.json");
	const bool bResult = ServerMapExporter::ExportSpawnLocationsToJsonFile(
		World,
		NAME_None,
		ChestActorTag,
		OutputPath,
		bDisableSpawnMarkerActorsAfterExport);

	UE_LOG(LogTemp, Log, TEXT("[ServerMapTest] ExportChestSpawnLocations result: %s"), bResult ? TEXT("true") : TEXT("false"));
	UE_LOG(LogTemp, Log, TEXT("[ServerMapTest] OutputPath: %s"), *OutputPath);
}

void AServerMapExportTestActor::ExportStoreAndChestSpawnLocations()
{
	UWorld* World = GetWorld();

	const FString OutputPath = FPaths::ProjectSavedDir() / TEXT("ServerMap/TestSpawnLocations.json");
	const bool bResult = ServerMapExporter::ExportSpawnLocationsToJsonFile(
		World,
		StoreActorTag,
		ChestActorTag,
		OutputPath,
		bDisableSpawnMarkerActorsAfterExport);

	UE_LOG(LogTemp, Log, TEXT("[ServerMapTest] ExportStoreAndChestSpawnLocations result: %s"), bResult ? TEXT("true") : TEXT("false"));
	UE_LOG(LogTemp, Log, TEXT("[ServerMapTest] OutputPath: %s"), *OutputPath);
}

void AServerMapExportTestActor::ExportMonsterSpawnLocations()
{
	UWorld* World = GetWorld();

	const FString OutputPath = FPaths::ProjectSavedDir() / TEXT("ServerMap/TestMonsterSpawnLocations.json");
	const bool bResult = ServerMapExporter::ExportMonsterSpawnLocationsToJsonFile(
		World,
		OutputPath,
		bDisableSpawnMarkerActorsAfterExport);

	UE_LOG(LogTemp, Log, TEXT("[ServerMapTest] ExportMonsterSpawnLocations result: %s"), bResult ? TEXT("true") : TEXT("false"));
	UE_LOG(LogTemp, Log, TEXT("[ServerMapTest] OutputPath: %s"), *OutputPath);
}

void AServerMapExportTestActor::ReplaceSelectedActorWithStaticMeshActors()
{
#if WITH_EDITOR
	UWorld* World = GetWorld();
	const int32 CreatedCount = ServerMapExporter::ReplaceSelectedActorWithStaticMeshActors(World, RequiredActorTag);

	UE_LOG(LogTemp, Log,
		TEXT("[ServerMapTest] ReplaceSelectedActorWithStaticMeshActors Created=%d"),
		CreatedCount);
#endif
}

void AServerMapExportTestActor::AddCollisionTagToSelectedActor()
{
#if WITH_EDITOR
	ServerMapExporter::AddCollisionTagToActors();

	UE_LOG(LogTemp, Log, TEXT("[ServerMapTest] AddCollisionTagToSelectedActor executed"));
#endif
}
