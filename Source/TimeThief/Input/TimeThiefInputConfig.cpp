#include "Input/TimeThiefInputConfig.h"
#include "InputAction.h"

const UInputAction* UTimeThiefInputConfig::FindNativeInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound) const {
	for (const FTimeThiefInputAction& Action : NativeInputActions) {
		if (Action.InputAction && Action.InputTag == InputTag) {
			return Action.InputAction;
		}
	}

	if (bLogNotFound) {
		UE_LOG(LogTemp, Error, TEXT("Can't find NativeInputAction for InputTag [%s] on InputConfig [%s]."), *InputTag.ToString(), *GetNameSafe(this));
	}

	return nullptr;
}

const UInputAction* UTimeThiefInputConfig::FindAbilityInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound) const {
	for (const FTimeThiefInputAction& Action : AbilityInputActions) {
		if (Action.InputAction && Action.InputTag == InputTag) {
			return Action.InputAction;
		}
	}

	if (bLogNotFound) {
		UE_LOG(LogTemp, Error, TEXT("Can't find AbilityInputAction for InputTag [%s] on InputConfig [%s]."), *InputTag.ToString(), *GetNameSafe(this));
	}

	return nullptr;
}