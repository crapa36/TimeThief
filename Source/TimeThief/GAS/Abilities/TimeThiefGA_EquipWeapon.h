#pragma once

#include "CoreMinimal.h"
#include "GAS/TimeThiefGameplayAbility.h"
#include "TimeThiefGA_EquipWeapon.generated.h"

class ATimeThiefWeaponBase;

UCLASS()
class TIMETHIEF_API UTimeThiefGA_EquipWeapon : public UTimeThiefGameplayAbility {
	GENERATED_BODY()

public:
	UTimeThiefGA_EquipWeapon();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TSubclassOf<ATimeThiefWeaponBase> WeaponClass;
};