#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "TimeThiefGameplayAbility.generated.h"

UENUM(BlueprintType)
enum class ETimeThiefAbilityInputID : uint8 {
	None,
	Confirm,
	Cancel
};

UCLASS()
class TIMETHIEF_API UTimeThiefGameplayAbility : public UGameplayAbility {
	GENERATED_BODY()

public:
	UTimeThiefGameplayAbility();

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	ETimeThiefAbilityInputID AbilityInputID;

	UFUNCTION(BlueprintPure, Category = "Ability")
	class ATimeThiefCharacterBase* GetTimeThiefCharacterFromActorInfo() const;
};