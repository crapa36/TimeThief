#pragma once

#include "CoreMinimal.h"
#include "Components/Combat/TimeThiefPawnCombatComponent.h"
#include "TimeThiefHeroCombatComponent.generated.h"

class ATimeThiefWeaponBase;

UCLASS()
class TIMETHIEF_API UTimeThiefHeroCombatComponent : public UTimeThiefPawnCombatComponent {
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "TimeThief|Combat")
	ATimeThiefWeaponBase* GetHeroCarriedWeaponByTag(FGameplayTag InWeaponTag) const;
};