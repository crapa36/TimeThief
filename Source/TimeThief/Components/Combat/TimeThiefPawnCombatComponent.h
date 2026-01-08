#pragma once

#include "CoreMinimal.h"
#include "Components/TimeThiefPawnExtensionComponent.h"
#include "GameplayTagContainer.h"
#include "TimeThiefPawnCombatComponent.generated.h"

class ATimeThiefWeaponBase;
class UAnimMontage;

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
	void EquipWeapon(FGameplayTag WeaponTag);

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Combat")
	void UnequipCurrentWeapon();
	
	UFUNCTION(BlueprintCallable, Category = "TimeThief|Combat")
	virtual void HandleInputPressed(FGameplayTag InputTag);

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Combat")
	virtual void HandleInputReleased(FGameplayTag InputTag);

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Combat")
	void AttachWeaponToSocket(ATimeThiefWeaponBase* Weapon);

	UPROPERTY(BlueprintReadWrite, Category = "TimeThief|Combat")
	FGameplayTag CurrentEquippedWeaponTag;

protected:
	UPROPERTY()
	TMap<FGameplayTag, TObjectPtr<ATimeThiefWeaponBase>> CharacterCarriedWeaponMap;

	UPROPERTY()
	TObjectPtr<ATimeThiefWeaponBase> CurrentEquippedWeapon;

	void PlayEquipMontage(ATimeThiefWeaponBase* Weapon);
};