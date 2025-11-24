#include "TimeThiefAssetManager.h"
#include "TimeThiefGameplayTags.h"

UTimeThiefAssetManager& UTimeThiefAssetManager::Get() {
	if (UTimeThiefAssetManager* Singleton = Cast<UTimeThiefAssetManager>(GEngine->AssetManager)) {
		return *Singleton;
	}
	return *NewObject<UTimeThiefAssetManager>();
}

void UTimeThiefAssetManager::StartInitialLoading() {
	Super::StartInitialLoading();
	// 여기서 태그 초기화 실행
	FTimeThiefGameplayTags::InitializeNativeGameplayTags();
}