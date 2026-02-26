#include "TimeThiefAssetManager.h"
#include "TimeThiefGameplayTags.h"

UTimeThiefAssetManager& UTimeThiefAssetManager::Get()
{
	check(GEngine);

	if (UTimeThiefAssetManager* Singleton = Cast<UTimeThiefAssetManager>(GEngine->AssetManager))
	{
		return *Singleton;
	}

	UE_LOG(LogTemp, Fatal, TEXT("Invalid AssetManagerClassName in DefaultEngine.ini. It must be UTimeThiefAssetManager!"));
	return *NewObject<UTimeThiefAssetManager>();
}

void UTimeThiefAssetManager::StartInitialLoading()
{
	Super::StartInitialLoading();
	FTimeThiefGameplayTags::InitializeNativeGameplayTags();
}