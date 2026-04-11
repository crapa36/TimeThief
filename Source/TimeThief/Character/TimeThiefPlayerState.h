// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "DataAssets/UpgradeData.h"
#include "GameFramework/PlayerState.h"
#include "TimeThiefPlayerState.generated.h"

/**
 * 
 */
USTRUCT(BlueprintType)
struct FStatus
{
	GENERATED_BODY()
	
	int Damage = 0;	
	int Stability = 0;
	int Capacity = 0;
	int Health = 0;
	int Speed = 0;
};

USTRUCT(BlueprintType)
struct FAppliedUpgradeStats
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float MoveSpeedBonus = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float JumpVelocityBonus = 0.0f;
};

USTRUCT(BlueprintType)
struct FAppliedWeaponUpgradeStats
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float DamageBonus = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 CapacityBonusAmmo = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float RecoilReduction = 0.0f;
};

UCLASS()
class TIMETHIEF_API ATimeThiefPlayerState : public APlayerState
{
	GENERATED_BODY()
	
public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FStatus Status;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FAppliedUpgradeStats AppliedUpgradeStats;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TMap<FGameplayTag, FAppliedWeaponUpgradeStats> AppliedWeaponUpgradeStatsMap;

	void RecalculateAppliedUpgradeStats(
		const TMap<FGameplayTag, FUpgradeFloatLevels>& DamageBonusByWeaponAndLevel,
		const TMap<FGameplayTag, FUpgradeIntLevels>& CapacityBonusByWeaponAndLevel,
		const TMap<FGameplayTag, FUpgradeFloatLevels>& RecoilReductionByWeaponAndLevel,
		const TArray<float>& MoveSpeedBonusPerLevel,
		const TArray<float>& JumpVelocityBonusPerLevel
	);

	const FAppliedWeaponUpgradeStats* GetAppliedWeaponUpgradeStats(const FGameplayTag& WeaponTag) const;
};
