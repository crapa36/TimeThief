#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "TimeThiefWeaponLoadoutData.generated.h"

USTRUCT(BlueprintType)
struct FTimeThiefWeaponLoadoutItem {
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FGameplayTag WeaponTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UTexture2D> WeaponIcon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FText WeaponName;
};

UCLASS(BlueprintType, Const)
class TIMETHIEF_API UTimeThiefWeaponLoadoutData : public UPrimaryDataAsset {
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (TitleProperty = "WeaponTag"))
	TArray<FTimeThiefWeaponLoadoutItem> WeaponLoadout;

	UFUNCTION(BlueprintPure, Category = "TimeThief|Weapon")
	bool ContainsWeapon(FGameplayTag InTag) const;

	UFUNCTION(BlueprintPure, Category = "TimeThief|Weapon")
	bool GetWeaponInfo(FGameplayTag InTag, FTimeThiefWeaponLoadoutItem& OutInfo) const;
};