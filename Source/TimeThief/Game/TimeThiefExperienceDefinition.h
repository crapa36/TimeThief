#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "TimeThiefExperienceDefinition.generated.h"

class UGameFeatureAction;
class UTimeThiefPawnData;

UCLASS(BlueprintType, Const)
class TIMETHIEF_API UTimeThiefExperienceDefinition : public UPrimaryDataAsset {
	GENERATED_BODY()

public:
	UTimeThiefExperienceDefinition();

	UPROPERTY(EditDefaultsOnly, Category = "Gameplay")
	TArray<FString> GameFeaturesToEnable;

	UPROPERTY(EditDefaultsOnly, Category = "Gameplay")
	TObjectPtr<UTimeThiefPawnData> DefaultPawnData;

	UPROPERTY(EditDefaultsOnly, Instanced, Category = "Actions")
	TArray<TObjectPtr<UGameFeatureAction>> Actions;

	UPROPERTY(EditDefaultsOnly, Category = "Gameplay", Meta = (Categories = "Experience"))
	FGameplayTagContainer ExperienceTags;

public:
#if WITH_EDITORONLY_DATA
	virtual void UpdateAssetBundleData() override;
#endif
};

