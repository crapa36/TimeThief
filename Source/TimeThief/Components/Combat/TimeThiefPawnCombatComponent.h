#pragma once

#include "CoreMinimal.h"
#include "Components/TimeThiefPawnExtensionComponent.h"
#include "GameplayTagContainer.h"
#include "TimeThiefPawnCombatComponent.generated.h"

class ATimeThiefMasterWeapon;
class UTimeThiefWeaponComponentBase;
class UAnimMontage;

struct FRemoteAttackNotify;
struct FCombatAttackRequest;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnWeaponEquippedSignature, UTimeThiefWeaponComponentBase*);
DECLARE_MULTICAST_DELEGATE(FOnWeaponUnequippedSignature);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCombatAttackRequestSignature, const FCombatAttackRequest&);

UCLASS(Blueprintable, ClassGroup = (TimeThief), meta = (BlueprintSpawnableComponent))
class TIMETHIEF_API UTimeThiefPawnCombatComponent : public UTimeThiefPawnExtensionComponent {
	GENERATED_BODY()

public:
	UTimeThiefPawnCombatComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Combat")
	virtual void EquipWeapon(FGameplayTag WeaponTag);

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Combat")
	virtual void UnequipCurrentWeapon();

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Combat")
	UTimeThiefWeaponComponentBase* GetCharacterCurrentEquippedWeapon() const;

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Combat")
	virtual void HandleInputPressed(FGameplayTag InputTag);

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Combat")
	virtual void HandleInputReleased(FGameplayTag InputTag);

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Combat")
	void AttachMasterWeaponToCharacter(FName SocketName);

	UFUNCTION(BlueprintPure, Category = "TimeThief|Combat")
	bool IsEquippingWeapon() const { return bIsEquippingWeapon; }

	UFUNCTION(BlueprintPure, Category = "TimeThief|Combat")
	bool IsAiming() const { return bIsAiming; }

	UPROPERTY(BlueprintReadWrite, Category = "TimeThief|Combat")
	FGameplayTag CurrentEquippedWeaponTag;

	FOnWeaponEquippedSignature OnWeaponEquipped_Delegate;
	FOnWeaponUnequippedSignature OnWeaponUnequipped_Delegate;
	FOnCombatAttackRequestSignature OnCombatAttackRequest_Delegate;
	// TODO: 공격 및 재장전 등의 액션을 취해야 할 때 델리게이트를 Broadcast 할 것
	
	virtual void Remote_AttackRequest(const FRemoteAttackNotify& AttackRequest);

	virtual void Remote_SyncAimingState(bool bNewAiming);
	virtual void Remote_SyncFireAction();
	virtual void Remote_SyncAimLocation(const FVector& NewAimLocation);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual void OnEquipAnimFinished();
	virtual void OnUnequipAnimFinished();

protected:
	virtual void BeginPlay() override;
	virtual void OnEquipFinished();

	void PlayFireMontage();
	float PlayEquipMontage(UTimeThiefWeaponComponentBase* Weapon);
	void ApplyCombatStateTag(FGameplayTag WeaponTag);
	void RemoveCombatStateTag(FGameplayTag WeaponTag);

	UPROPERTY(EditDefaultsOnly, Category = "TimeThief|Combat")
	TSubclassOf<ATimeThiefMasterWeapon> MasterWeaponClass;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "TimeThief|Combat")
	TObjectPtr<ATimeThiefMasterWeapon> MasterWeaponPtr;

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

private:
	void SpawnMasterWeapon();
};