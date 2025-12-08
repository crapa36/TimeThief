#pragma once

#include "CoreMinimal.h"
#include "Components/TimeThiefPawnExtensionComponent.h"
#include "GameplayTagContainer.h"
#include "TimeThiefPawnCombatComponent.generated.h"

class ATimeThiefWeaponBase;

UENUM(BlueprintType)
enum class EToggleDamageType : uint8 {
	CurrentEquippedWeapon,
	LeftHand,
	RightHand
};

UCLASS()
class TIMETHIEF_API UTimeThiefPawnCombatComponent : public UTimeThiefPawnExtensionComponent {
	GENERATED_BODY()

public:
	UTimeThiefPawnCombatComponent();

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Combat")
	void RegisterSpawnedWeapon(FGameplayTag InWeaponTagToRegister, ATimeThiefWeaponBase* InWeaponToRegister, bool bRegisterAsEquippedWeapon = false);

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Combat")
	ATimeThiefWeaponBase* GetCharacterCarriedWeaponByTag(FGameplayTag InWeaponTagToGet) const;

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Combat")
	ATimeThiefWeaponBase* GetCharacterCurrentEquippedWeapon() const;

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Combat")
	void ToggleWeaponCollision(bool bShouldEnable, EToggleDamageType ToggleDamageType = EToggleDamageType::CurrentEquippedWeapon);

	UPROPERTY(BlueprintReadWrite, Category = "TimeThief|Combat")
	FGameplayTag CurrentEquippedWeaponTag;

private:
	UPROPERTY()
	TMap<FGameplayTag, TObjectPtr<ATimeThiefWeaponBase>> CharacterCarriedWeaponMap;

	UPROPERTY()
	TObjectPtr<ATimeThiefWeaponBase> CurrentEquippedWeapon;
};