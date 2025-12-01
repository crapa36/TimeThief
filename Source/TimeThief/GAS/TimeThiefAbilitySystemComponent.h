#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "TimeThiefAbilitySystemComponent.generated.h"

UCLASS()
class TIMETHIEF_API UTimeThiefAbilitySystemComponent : public UAbilitySystemComponent {
	GENERATED_BODY()

public:
	void AbilityInputTagPressed(const FGameplayTag& InputTag);
	void AbilityInputTagReleased(const FGameplayTag& InputTag);

protected:
	void ProcessAbilityInput(const FGameplayTag& InputTag, bool bPressed);
};