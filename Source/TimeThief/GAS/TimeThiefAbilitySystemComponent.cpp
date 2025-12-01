#include "GAS/TimeThiefAbilitySystemComponent.h"
#include "GAS/TimeThiefGameplayAbility.h"

void UTimeThiefAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag) {
	if (InputTag.IsValid()) {
		ProcessAbilityInput(InputTag, true);
	}
}

void UTimeThiefAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag) {
	if (InputTag.IsValid()) {
		ProcessAbilityInput(InputTag, false);
	}
}

void UTimeThiefAbilitySystemComponent::ProcessAbilityInput(const FGameplayTag& InputTag, bool bPressed) {
	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities()) {
		if (AbilitySpec.DynamicAbilityTags.HasTagExact(InputTag)) {
			if (bPressed) {
				AbilitySpecInputPressed(AbilitySpec);
				if (!AbilitySpec.IsActive()) {
					TryActivateAbility(AbilitySpec.Handle);
				}
			}
			else {
				AbilitySpecInputReleased(AbilitySpec);
			}
		}
	}
}