#include "TimeThiefAssetManager.h"
#include "TimeThiefGameplayTags.h"

UTimeThiefAssetManager& UTimeThiefAssetManager::Get() {
	check(GEngine);

	if (UTimeThiefAssetManager* Singleton = Cast<UTimeThiefAssetManager>(GEngine->AssetManager)) {
		return *Singleton;
	}

	// [FATAL] AssetManager config missmatch. Check DefaultEngine.ini
	UE_LOG(LogTemp, Fatal, TEXT("Invalid AssetManagerClassName in DefaultEngine.ini. It must be UTimeThiefAssetManager!"));
	return *NewObject<UTimeThiefAssetManager>(); // Dummy return to satisfy compiler
}

void UTimeThiefAssetManager::StartInitialLoading() {
	Super::StartInitialLoading();

	// Load Native Tags
	FTimeThiefGameplayTags::InitializeNativeGameplayTags();
}