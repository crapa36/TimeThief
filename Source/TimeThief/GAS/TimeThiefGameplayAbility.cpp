#include "GAS/TimeThiefGameplayAbility.h"
#include "Character/TimeThiefCharacterBase.h"
#include "AbilitySystemComponent.h"

UTimeThiefGameplayAbility::UTimeThiefGameplayAbility() {
	AbilityInputID = ETimeThiefAbilityInputID::None;
}

ATimeThiefCharacterBase* UTimeThiefGameplayAbility::GetTimeThiefCharacterFromActorInfo() const {
	return (CurrentActorInfo ? Cast<ATimeThiefCharacterBase>(CurrentActorInfo->AvatarActor) : nullptr);
}