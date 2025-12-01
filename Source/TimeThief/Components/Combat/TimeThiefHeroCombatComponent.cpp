#include "Components/Combat/TimeThiefHeroCombatComponent.h"
#include "Weapon/TimeThiefWeaponBase.h"

ATimeThiefWeaponBase* UTimeThiefHeroCombatComponent::GetHeroCarriedWeaponByTag(FGameplayTag InWeaponTag) const {
	return Cast<ATimeThiefWeaponBase>(GetCharacterCarriedWeaponByTag(InWeaponTag));
}