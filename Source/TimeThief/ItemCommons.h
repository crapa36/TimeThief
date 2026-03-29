#pragma once

UENUM(BlueprintType)
enum class EItemCategory : uint8
{
	WeaponUpgrade UMETA(DisplayName = "Weapon Upgrade"),
	CharacterUpgrade UMETA(DisplayName = "Character Upgrade"),
	Skill UMETA(DisplayName = "Skill"),
	Consumable UMETA(DisplayName = "Consumable"),
	Throwable UMETA(DisplayName = "Throwable"),
};

UENUM(BlueprintType)
enum class EItemID : uint8
{
	CapacityUpgrade UMETA(DisplayName = "Capacity Upgrade"),
	DamageUpgrade UMETA(DisplayName = "Damage Upgrade"),
	StabilityUpgrade UMETA(DisplayName = "Stability Upgrade"),
	HealthUpgrade UMETA(DisplayName = "Health Upgrade"),
	SpeedUpgrade UMETA(DisplayName = "Speed Upgrade"),
	Skill1 UMETA(DisplayName = "Skill 1"),
	Skill2 UMETA(DisplayName = "Skill 2"),
	Skill3 UMETA(DisplayName = "Skill 3"),
	SmallPotion UMETA(DisplayName = "Small Potion"),
	BigPotion UMETA(DisplayName = "Big Potion"),
	SkillBoost UMETA(DisplayName = "Skill Boost"),
	Grenade UMETA(DisplayName = "Grenade"),
	TimePoint UMETA(DisplayName = "Time Point"),
	SIZE UMETA(Hidden)
};

ENUM_RANGE_BY_COUNT(EItemID, EItemID::SIZE);

struct FStoreOrder
{
	EItemID ItemID;
	
	int Price;
};

struct FVicinityItemData
{
	EItemID ItemID;
	
	int Quantity;
};
