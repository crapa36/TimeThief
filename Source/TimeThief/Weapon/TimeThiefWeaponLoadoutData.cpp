#include "Weapon/TimeThiefWeaponLoadoutData.h"

bool UTimeThiefWeaponLoadoutData::ContainsWeapon(FGameplayTag InTag) const {
	for (const FTimeThiefWeaponLoadoutItem& Item : WeaponLoadout) {
		if (Item.WeaponTag == InTag) {
			return true;
		}
	}
	return false;
}

bool UTimeThiefWeaponLoadoutData::GetWeaponInfo(FGameplayTag InTag, FTimeThiefWeaponLoadoutItem& OutInfo) const {
	for (const FTimeThiefWeaponLoadoutItem& Item : WeaponLoadout) {
		if (Item.WeaponTag == InTag) {
			OutInfo = Item;
			return true;
		}
	}
	return false;
}