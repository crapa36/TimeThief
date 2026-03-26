


#include "ServerMapExportTestActor.h"

#include "ServerMapExporter.h"


// Sets default values
AServerMapExportTestActor::AServerMapExportTestActor()
{
#if WITH_EDITORONLY_DATA
	bIsEditorOnlyActor = true;
#endif
}

void AServerMapExportTestActor::ExportSelectedBox()
{
	const FString OutputPath = FPaths::ProjectSavedDir() / TEXT("ServerMap/TestMap.servermap");
	const bool bResult = ServerMapExporter::ExportSelectedActorBoxesToFile(OutputPath);

	UE_LOG(LogTemp, Log, TEXT("[ServerMapTest] Export result: %s"), bResult ? TEXT("true") : TEXT("false"));
	UE_LOG(LogTemp, Log, TEXT("[ServerMapTest] OutputPath: %s"), *OutputPath);
}

void AServerMapExportTestActor::ExportTaggedBoxes()
{
	const FString OutputPath = FPaths::ProjectSavedDir() / TEXT("ServerMap/TestMap_Tagged.servermap");
	const bool bResult = ServerMapExporter::ExportActorsWithTagToFile(GetWorld(), TEXT("ServerCollision"), OutputPath);

	UE_LOG(LogTemp, Log, TEXT("[ServerMapTest] ExportTaggedBoxes result: %s"), bResult ? TEXT("true") : TEXT("false"));
	UE_LOG(LogTemp, Log, TEXT("[ServerMapTest] OutputPath: %s"), *OutputPath);
}


