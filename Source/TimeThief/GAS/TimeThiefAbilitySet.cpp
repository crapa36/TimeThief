#include "GAS/TimeThiefAbilitySet.h"
#include "AbilitySystemComponent.h"
#include "GAS/TimeThiefGameplayAbility.h"

void UTimeThiefAbilitySet::GiveToAbilitySystem(UAbilitySystemComponent* ASC, UObject* SourceObject) const {
	if (!ASC) return;

	for (const FTimeThiefAbilitySetItem& AbilityToGrant : GrantedAbilities) {
		if (!AbilityToGrant.GameplayAbility) continue;

		FGameplayAbilitySpec AbilitySpec(AbilityToGrant.GameplayAbility, 1);
		AbilitySpec.SourceObject = SourceObject;

		if (AbilityToGrant.InputTag.IsValid()) {
			
			AbilitySpec.GetDynamicSpecSourceTags().AddTag(AbilityToGrant.InputTag);
		}

		ASC->GiveAbility(AbilitySpec);
	}
}