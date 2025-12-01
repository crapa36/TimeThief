#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "TimeThiefAbilitySet.generated.h"

class UTimeThiefGameplayAbility;
class UAbilitySystemComponent;

USTRUCT(BlueprintType)
struct FTimeThiefAbilitySetItem {
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UTimeThiefGameplayAbility> GameplayAbility;

	UPROPERTY(EditDefaultsOnly)
	FGameplayTag InputTag;
};

UCLASS()
class TIMETHIEF_API UTimeThiefAbilitySet : public UPrimaryDataAsset {
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Abilities")
	TArray<FTimeThiefAbilitySetItem> GrantedAbilities;

	void GiveToAbilitySystem(UAbilitySystemComponent* ASC, UObject* SourceObject = nullptr) const;
};