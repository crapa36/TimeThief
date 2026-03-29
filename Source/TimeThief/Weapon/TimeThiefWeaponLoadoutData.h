#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "TimeThiefWeaponLoadoutData.generated.h"

class ATimeThiefWeaponBase;

UCLASS(BlueprintType)
class TIMETHIEF_API UTimeThiefWeaponLoadoutData : public UPrimaryDataAsset {
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon")
	TMap<FGameplayTag, TSubclassOf<ATimeThiefWeaponBase>> WeaponMap;
};