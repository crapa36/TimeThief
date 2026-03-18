#pragma once

UENUM(BlueprintType)
enum class EStoreItemType : uint8
{
	WeaponUpgrade UMETA(DisplayName = "Weapon Upgrade"),
	CharacterUpgrade UMETA(DisplayName = "Character Upgrade"),
	Skill UMETA(DisplayName = "Skill"),
	Consumable UMETA(DisplayName = "Consumable")
};

UENUM(BlueprintType)
enum class EStoreItemName : uint8
{
	CapacityUpgrade UMETA(DisplayName = "Capacity Upgrade"),
	DamageUpgrade UMETA(DisplayName = "Damage Upgrade"),
	StabilityUpgrade UMETA(DisplayName = "Stability Upgrade"),
	HealthUpgrade UMETA(DisplayName = "Health Upgrade"),
	SpeedUpgrade UMETA(DisplayName = "Speed Upgrade"),
	Skill1 UMETA(DisplayName = "Skill 1"),
	Skill2 UMETA(DisplayName = "Skill 2"),
	Skill3 UMETA(DisplayName = "Skill 3"),
	Potion UMETA(DisplayName = "Potion"),
	SIZE UMETA(Hidden)
};

ENUM_RANGE_BY_COUNT(EStoreItemName, EStoreItemName::SIZE);

struct FStoreOrder
{
	EStoreItemName ItemName;
	
	int Price;
};
