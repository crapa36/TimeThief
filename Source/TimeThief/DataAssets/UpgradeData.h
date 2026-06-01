#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "UpgradeData.generated.h"

USTRUCT(BlueprintType)
struct FUpgradeFloatLevels
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<float> Values;
};

USTRUCT(BlueprintType)
struct FUpgradeIntLevels
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<int32> Values;
};

UCLASS()
class TIMETHIEF_API UUpgradeData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UUpgradeData();

	// Level arrays are additive deltas: final value = base + sum(level deltas)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TimeThief|Upgrade|Character")
	TArray<float> MoveSpeedBonusPerLevel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TimeThief|Upgrade|Character")
	TArray<float> JumpVelocityBonusPerLevel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TimeThief|Upgrade|Weapon")
	TMap<FGameplayTag, FUpgradeFloatLevels> DamageBonusByWeaponAndLevel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TimeThief|Upgrade|Weapon")
	TMap<FGameplayTag, FUpgradeIntLevels> CapacityBonusByWeaponAndLevel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TimeThief|Upgrade|Weapon")
	TMap<FGameplayTag, FUpgradeFloatLevels> RecoilReductionByWeaponAndLevel;
};

