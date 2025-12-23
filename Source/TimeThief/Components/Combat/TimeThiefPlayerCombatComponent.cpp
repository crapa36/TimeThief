#include "Components/Combat/TimeThiefPlayerCombatComponent.h"
#include "Weapon/TimeThiefWeaponBase.h"

ATimeThiefWeaponBase* UTimeThiefPlayerCombatComponent::GetPlayerCarriedWeaponByTag(FGameplayTag InWeaponTag) const {
	return Cast<ATimeThiefWeaponBase>(GetCharacterCarriedWeaponByTag(InWeaponTag));
}