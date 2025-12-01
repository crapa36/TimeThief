#include "Components/Combat/TimeThiefPawnCombatComponent.h"
#include "Weapon/TimeThiefWeaponBase.h"

void UTimeThiefPawnCombatComponent::RegisterSpawnedWeapon(FGameplayTag InWeaponTagToRegister, ATimeThiefWeaponBase* InWeaponToRegister, bool bRegisterAsEquippedWeapon) {
	if (CharacterCarriedWeaponMap.Contains(InWeaponTagToRegister)) {
		return;
	}

	CharacterCarriedWeaponMap.Emplace(InWeaponTagToRegister, InWeaponToRegister);

	if (bRegisterAsEquippedWeapon) {
		CurrentEquippedWeaponTag = InWeaponTagToRegister;
	}
}

ATimeThiefWeaponBase* UTimeThiefPawnCombatComponent::GetCharacterCarriedWeaponByTag(FGameplayTag InWeaponTagToGet) const {
	if (CharacterCarriedWeaponMap.Contains(InWeaponTagToGet)) {
		return *CharacterCarriedWeaponMap.Find(InWeaponTagToGet);
	}
	return nullptr;
}

ATimeThiefWeaponBase* UTimeThiefPawnCombatComponent::GetCharacterCurrentEquippedWeapon() const {
	if (!CurrentEquippedWeaponTag.IsValid()) {
		return nullptr;
	}
	return GetCharacterCarriedWeaponByTag(CurrentEquippedWeaponTag);
}