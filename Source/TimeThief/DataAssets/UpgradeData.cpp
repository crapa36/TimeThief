#include "UpgradeData.h"
#include "TimeThiefGameplayTags.h"

UUpgradeData::UUpgradeData()
{
	const FTimeThiefGameplayTags& Tags = FTimeThiefGameplayTags::Get();

		MoveSpeedBonusPerLevel = {50.0f, 50.0f, 50.0f, 50.0f, 50.0f};

		JumpVelocityBonusPerLevel = {30.0f, 30.0f, 30.0f, 30.0f, 30.0f};

		FUpgradeFloatLevels RifleDamageLevels;
		RifleDamageLevels.Values = {2.0f, 2.0f, 3.0f, 3.0f, 4.0f};
		DamageBonusByWeaponAndLevel.Add(Tags.Weapon_Rifle, RifleDamageLevels);

		FUpgradeFloatLevels ShotgunDamageLevels;
		ShotgunDamageLevels.Values = {0.5f, 0.5f, 1.0f, 1.0f, 1.5f};
		DamageBonusByWeaponAndLevel.Add(Tags.Weapon_Shotgun, ShotgunDamageLevels);

		FUpgradeFloatLevels RocketDamageLevels;
		RocketDamageLevels.Values = {25.0f, 25.0f, 30.0f, 30.0f, 40.0f};
		DamageBonusByWeaponAndLevel.Add(Tags.Weapon_RocketLauncher, RocketDamageLevels);

		FUpgradeIntLevels RifleCapacityLevels;
		RifleCapacityLevels.Values = {5, 5, 5, 5, 5};
		CapacityBonusByWeaponAndLevel.Add(Tags.Weapon_Rifle, RifleCapacityLevels);

		FUpgradeIntLevels ShotgunCapacityLevels;
		ShotgunCapacityLevels.Values = {1, 1, 1, 1, 1};
		CapacityBonusByWeaponAndLevel.Add(Tags.Weapon_Shotgun, ShotgunCapacityLevels);

		FUpgradeIntLevels RocketCapacityLevels;
		RocketCapacityLevels.Values = {0, 0, 1, 0, 1};
		CapacityBonusByWeaponAndLevel.Add(Tags.Weapon_RocketLauncher, RocketCapacityLevels);

		FUpgradeFloatLevels RifleRecoilLevels;
		RifleRecoilLevels.Values = {0.05f, 0.05f, 0.05f, 0.05f, 0.05f};
		RecoilReductionByWeaponAndLevel.Add(Tags.Weapon_Rifle, RifleRecoilLevels);

		FUpgradeFloatLevels ShotgunRecoilLevels;
		ShotgunRecoilLevels.Values = {0.2f, 0.2f, 0.2f, 0.2f, 0.2f};
		RecoilReductionByWeaponAndLevel.Add(Tags.Weapon_Shotgun, ShotgunRecoilLevels);

		FUpgradeFloatLevels RocketRecoilLevels;
		RocketRecoilLevels.Values = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
		RecoilReductionByWeaponAndLevel.Add(Tags.Weapon_RocketLauncher, RocketRecoilLevels);
	
}


