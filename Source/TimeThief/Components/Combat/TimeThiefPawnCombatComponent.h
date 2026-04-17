#pragma once

#include "CoreMinimal.h"
#include "Components/TimeThiefPawnExtensionComponent.h"
#include "GameplayTagContainer.h"
#include "TimeThiefPawnCombatComponent.generated.h"

class ATimeThiefMasterWeapon;
class UNetworkCombatSyncComponent;
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
	
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void OnRegister() override;
	
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

	void BroadcastCombatAttackRequest(const FCombatAttackRequest& AttackRequest);

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
	virtual void Remote_SyncAimLocation(const FVector& Origin, const FVector& Direction);
	virtual void Remote_SyncFireAction();

	virtual void OnEquipAnimFinished();
	virtual void OnUnequipAnimFinished();

protected:
	virtual void OnEquipFinished();

	void PlayFireMontage();
	float PlayEquipMontage(UTimeThiefWeaponComponentBase* Weapon);
	void ApplyCombatStateTag(FGameplayTag WeaponTag);
	void RemoveCombatStateTag(FGameplayTag WeaponTag);

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

	FTimerHandle EquipTimerHandle;
	FVector CachedRemoteShotOrigin = FVector::ZeroVector;
	FVector CachedRemoteAimLocation = FVector::ZeroVector;
	FVector CachedRemoteAimDirection = FVector::ForwardVector;
	int32 RemoteFireNotifyCount = 0;
	int32 RemoteFireWeaponCorrectionCount = 0;

private:
	UPROPERTY(Transient)
	TObjectPtr<UNetworkCombatSyncComponent> CachedCombatSyncComponent = nullptr;
};