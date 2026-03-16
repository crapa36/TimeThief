#pragma once

UENUM(BlueprintType)
enum class EItemName : uint8
{
	SmallPotion = 0 UMETA(DisplayName = "Small Potion"),
	BigPotion UMETA(DisplayName = "Big Potion"),
	
	SIZE UMETA(Hidden)
};

ENUM_RANGE_BY_COUNT(EItemName, EItemName::SIZE)