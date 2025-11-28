#include "GAS/TimeThiefGameplayAbility.h"
#include "Character/TimeThiefCharacterBase.h"

UTimeThiefGameplayAbility::UTimeThiefGameplayAbility() {
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

ATimeThiefCharacterBase* UTimeThiefGameplayAbility::GetTimeThiefCharacterFromActorInfo() const {
	return Cast<ATimeThiefCharacterBase>(GetAvatarActorFromActorInfo());
}