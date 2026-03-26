#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ServerMapExportTestActor.generated.h"

class UServerCollisionPresetDataAsset;

UCLASS()
class TIMETHIEF_API AServerMapExportTestActor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AServerMapExportTestActor();
	
public:
	UFUNCTION(CallInEditor, Category = "ServerMap")
	void ExportSelectedBox();
	
	UFUNCTION(CallInEditor, Category = "ServerMap")
	void ExportTaggedShapes();
	
	UFUNCTION(CallInEditor, Category = "ServerMap")
	void GenerateShapesFromSelectedStaticMesh();

	UPROPERTY(EditAnywhere, Category = "ServerMap")
	TObjectPtr<UServerCollisionPresetDataAsset> TargetPresetAsset = nullptr;

	UFUNCTION(CallInEditor, Category = "ServerMap")
	void SaveSelectedGeneratedShapesToPreset();
	
	UFUNCTION(CallInEditor, Category = "ServerMap")
	void ExportSelectedActorUsingPreset();
	
};
