#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ServerMapExportTestActor.generated.h"

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

};
