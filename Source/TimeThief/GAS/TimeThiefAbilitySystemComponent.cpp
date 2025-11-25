#include "GAS/TimeThiefAbilitySystemComponent.h"

void UTimeThiefAbilitySystemComponent::AddCharacterAbilities(const TArray<TSubclassOf<UGameplayAbility>>& StartupAbilities) {
	if (bCharacterAbilitiesGiven) return;

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : StartupAbilities) {
		FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityClass, 1);
		GiveAbility(AbilitySpec);
	}
	bCharacterAbilitiesGiven = true;
}

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
	// 태그와 일치하는 어빌리티 스펙을 찾아 입력을 활성화/비활성화 처리
	// 추후 구현 예정
}