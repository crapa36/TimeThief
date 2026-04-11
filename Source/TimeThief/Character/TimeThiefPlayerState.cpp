// Fill out your copyright notice in the Description page of Project Settings.


#include "TimeThiefPlayerState.h"

void ATimeThiefPlayerState::OnBeginRespawn()
{
	ILifeObserver::OnBeginRespawn();
	
	Status = SaveStatus;
}

namespace
{
	template <typename TValue>
	TValue GetAccumulatedValueForLevel(const TArray<TValue>& LevelValues, int32 Level, const TValue& DefaultValue)
	{
		if (Level <= 0 || LevelValues.Num() == 0)
		{
			return DefaultValue;
		}

		TValue AccumulatedValue = DefaultValue;
		const int32 Count = FMath::Min(Level, LevelValues.Num());
		for (int32 Index = 0; Index < Count; ++Index)
		{
			AccumulatedValue += LevelValues[Index];
		}

		return AccumulatedValue;
	}
}

void ATimeThiefPlayerState::RecalculateAppliedUpgradeStats(
	const TMap<FGameplayTag, FUpgradeFloatLevels>& DamageBonusByWeaponAndLevel,
	const TMap<FGameplayTag, FUpgradeIntLevels>& CapacityBonusByWeaponAndLevel,
	const TMap<FGameplayTag, FUpgradeFloatLevels>& RecoilReductionByWeaponAndLevel,
	const TArray<float>& MoveSpeedBonusPerLevel,
	const TArray<float>& JumpVelocityBonusPerLevel
)
{
	AppliedUpgradeStats.MoveSpeedBonus = FMath::Max(0.0f, GetAccumulatedValueForLevel<float>(MoveSpeedBonusPerLevel, Status.Speed, 0.0f));
	AppliedUpgradeStats.JumpVelocityBonus = FMath::Max(0.0f, GetAccumulatedValueForLevel<float>(JumpVelocityBonusPerLevel, Status.Speed, 0.0f));

	AppliedWeaponUpgradeStatsMap.Reset();

	for (const TPair<FGameplayTag, FUpgradeFloatLevels>& Pair : DamageBonusByWeaponAndLevel)
	{
		const float DamageBonus = FMath::Max(0.0f, GetAccumulatedValueForLevel<float>(Pair.Value.Values, Status.Damage, 0.0f));
		AppliedWeaponUpgradeStatsMap.FindOrAdd(Pair.Key).DamageBonus = DamageBonus;
	}

	for (const TPair<FGameplayTag, FUpgradeIntLevels>& Pair : CapacityBonusByWeaponAndLevel)
	{
		const int32 CapacityBonus = FMath::Max(0, GetAccumulatedValueForLevel<int32>(Pair.Value.Values, Status.Capacity, 0));
		AppliedWeaponUpgradeStatsMap.FindOrAdd(Pair.Key).CapacityBonusAmmo = CapacityBonus;
	}

	for (const TPair<FGameplayTag, FUpgradeFloatLevels>& Pair : RecoilReductionByWeaponAndLevel)
	{
		const float RecoilReduction = FMath::Max(0.0f, GetAccumulatedValueForLevel<float>(Pair.Value.Values, Status.Stability, 0.0f));
		AppliedWeaponUpgradeStatsMap.FindOrAdd(Pair.Key).RecoilReduction = RecoilReduction;
	}
}

const FAppliedWeaponUpgradeStats* ATimeThiefPlayerState::GetAppliedWeaponUpgradeStats(const FGameplayTag& WeaponTag) const
{
	return AppliedWeaponUpgradeStatsMap.Find(WeaponTag);
}

