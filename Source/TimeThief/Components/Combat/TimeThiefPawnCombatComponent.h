#pragma once

#include "CoreMinimal.h"
#include "Components/TimeThiefPawnExtensionComponent.h"
#include "GameplayTagContainer.h"
#include "TimeThiefPawnCombatComponent.generated.h"

class ATimeThiefWeaponBase;

UCLASS()
class TIMETHIEF_API UTimeThiefPawnCombatComponent : public UTimeThiefPawnExtensionComponent {
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "TimeThief|Combat")
	void RegisterSpawnedWeapon(FGameplayTag InWeaponTagToRegister, ATimeThiefWeaponBase* InWeaponToRegister, bool bRegisterAsEquippedWeapon = false);

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Combat")
	ATimeThiefWeaponBase* GetCharacterCarriedWeaponByTag(FGameplayTag InWeaponTagToGet) const;

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Combat")
	ATimeThiefWeaponBase* GetCharacterCurrentEquippedWeapon() const;

	UPROPERTY(BlueprintReadWrite, Category = "TimeThief|Combat")
	FGameplayTag CurrentEquippedWeaponTag;

private:
	TMap<FGameplayTag, ATimeThiefWeaponBase*> CharacterCarriedWeaponMap;
};