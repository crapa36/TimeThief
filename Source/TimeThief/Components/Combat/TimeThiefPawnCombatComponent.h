#pragma once

#include "CoreMinimal.h"
#include "Components/TimeThiefPawnExtensionComponent.h"
#include "GameplayTagContainer.h"
#include "TimeThiefPawnCombatComponent.generated.h"

class ATimeThiefWeaponBase;
class UAnimMontage;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnWeaponEquippedSignature, ATimeThiefWeaponBase*);
DECLARE_MULTICAST_DELEGATE(FOnWeaponUnequippedSignature);

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

	UFUNCTION(BlueprintPure, Category = "TimeThief|Combat")
	bool IsEquippingWeapon() const { return bIsEquippingWeapon; }

	UFUNCTION(BlueprintPure, Category = "TimeThief|Combat")
	bool IsAiming() const { return bIsAiming; }

	UPROPERTY(BlueprintReadWrite, Category = "TimeThief|Combat")
	FGameplayTag CurrentEquippedWeaponTag;

	FOnWeaponEquippedSignature OnWeaponEquipped_Delegate;
	FOnWeaponUnequippedSignature OnWeaponUnequipped_Delegate;

	virtual void Remote_SyncAimingState(bool bNewAiming);
	virtual void Remote_SyncFireAction();

	virtual void Remote_SyncAimLocation(const FVector& NewAimLocation);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	UPROPERTY()
	TMap<FGameplayTag, TObjectPtr<ATimeThiefWeaponBase>> CharacterCarriedWeaponMap;

	UPROPERTY()
	TObjectPtr<ATimeThiefWeaponBase> CurrentEquippedWeapon;

	UPROPERTY(EditDefaultsOnly, Category = "TimeThief|Combat")
	TMap<FGameplayTag, FGameplayTag> WeaponToStateTagMap;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|Combat")
	bool bIsEquippingWeapon = false;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|Combat")
	bool bIsAiming = false;

	UPROPERTY(EditDefaultsOnly, Category = "TimeThief|Combat")
	UAnimMontage* FireMontage;

	FVector TargetAimLocation;
	FVector RemoteTargetAimLocation;

	FTimerHandle EquipTimerHandle;

	virtual void OnEquipFinished();
	void PlayFireMontage();
	float PlayEquipMontage(ATimeThiefWeaponBase* Weapon);
	void ApplyCombatStateTag(FGameplayTag WeaponTag);
	void RemoveCombatStateTag(FGameplayTag WeaponTag);
};