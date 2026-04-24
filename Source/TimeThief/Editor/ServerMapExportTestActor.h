#pragma once

#include "CoreMinimal.h"
#include "ServerMapTags.h"
#include "GameFramework/Actor.h"
#include "ServerMapExportTestActor.generated.h"

class UServerCollisionPresetDataAsset;

UCLASS()
class TIMETHIEF_API AServerMapExportTestActor : public AActor
{
	GENERATED_BODY()

public:
	AServerMapExportTestActor();

public:
	// =============================
	// Shared Settings
	// =============================

	UPROPERTY(EditAnywhere, Category = "ServerMap|Settings", meta = (ToolTip = "Folder path where collision preset assets will be created or updated."))
	FString PresetFolderPath = TEXT("/Game/ServerMap/Presets");

	UPROPERTY(EditAnywhere, Category = "ServerMap|Settings", meta = (ToolTip = "If true, only generated shapes are saved to presets. If false, ManualApproved shapes are also included."))
	bool bOnlySaveGeneratedShapes = true;

	UPROPERTY(EditAnywhere, Category = "ServerMap|Settings", meta = (ToolTip = "If true, existing generated shapes are removed before regeneration."))
	bool bClearExistingGeneratedShapesBeforeRegenerate = true;

public:
	// =============================
	// Selected Actor Workflow
	// =============================

	UFUNCTION(CallInEditor, Category = "ServerMap|Selected")
	void GenerateShapesForSelectedActor();

	UFUNCTION(CallInEditor, Category = "ServerMap|Selected")
	void LoadPresetShapesForSelectedActor();

	UFUNCTION(CallInEditor, Category = "ServerMap|Selected")
	void ApproveSelectedActorGeneratedShapes();
	
	UFUNCTION(CallInEditor, Category = "ServerMap|Selected")
	void SaveSelectedActorShapesToPresetAndClearWorldShapes();
	
	UFUNCTION(CallInEditor, Category = "ServerMap|Selected")
	void ClearGeneratedShapesForSelectedActor();
	
	UFUNCTION(CallInEditor, Category = "ServerMap|Selected")
	void ValidateSelectedActor();
	
	UFUNCTION(CallInEditor, Category = "ServerMap|Selected")
	void ExportSelectedActorResolved();

public:
	// =============================
	// Batch Settings
	// =============================

	UPROPERTY(EditAnywhere, Category = "ServerMap|Batch", meta = (ToolTip = "Actor tag used to collect collision authoring targets in the current level."))
	FName RequiredActorTag = ServerTags::Collision;

	UPROPERTY(EditAnywhere, Category = "ServerMap|Batch", meta = (ToolTip = "Skip actors that already contain valid shape components."))
	bool bSkipActorsWithExistingShapes = true;

	UPROPERTY(EditAnywhere, Category = "ServerMap|Batch", meta = (ToolTip = "Skip actors when all valid static meshes already have presets."))
	bool bSkipActorsWithExistingPreset = false;

public:
	// =============================
	// Batch Workflow
	// =============================

	UFUNCTION(CallInEditor, Category = "ServerMap|Batch")
	void GenerateShapesForTaggedActors();

	UFUNCTION(CallInEditor, Category = "ServerMap|Batch")
	void ClearGeneratedShapesForTaggedActors();

	UFUNCTION(CallInEditor, Category = "ServerMap|Batch")
	void SaveTaggedGeneratedShapesToPresets();

	UFUNCTION(CallInEditor, Category = "ServerMap|Batch")
	void ValidateTaggedActors();

	UFUNCTION(CallInEditor, Category = "ServerMap|Batch")
	void ExportTaggedActorsResolved();
	
};
