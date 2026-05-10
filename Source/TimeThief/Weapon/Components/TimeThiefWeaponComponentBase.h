#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Weapon/WeaponStatData.h"
#include "TimeThiefWeaponComponentBase.generated.h"

class UAnimInstance;
class UAnimMontage;
class UAnimSequenceBase;
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;

enum ECombatNotifyType : uint8;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnWeaponAmmoChangedSignature, int32, int32);

UCLASS(Blueprintable, ClassGroup = (TimeThief), meta = (BlueprintSpawnableComponent))
class TIMETHIEF_API UTimeThiefWeaponComponentBase : public UActorComponent
{
	GENERATED_BODY()

public:
	UTimeThiefWeaponComponentBase();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void SetWeaponTag(FGameplayTag InTag) { WeaponTag = InTag; }
	void SetWeaponMeshAsset(UStaticMesh* InMesh) { WeaponMeshAsset = InMesh; }

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Weapon")
	virtual void StartFire();

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Weapon")
	virtual void StopFire();

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Weapon")
	virtual void Reload();

	UFUNCTION(BlueprintPure, Category = "TimeThief|Weapon")
	virtual bool CanFire() const;

	UFUNCTION(BlueprintPure, Category = "TimeThief|Weapon")
	virtual bool CanReload() const;

	UFUNCTION(BlueprintPure, Category = "TimeThief|Weapon")
	int32 GetCurrentAmmo() const { return CurrentAmmo; }

	UFUNCTION(BlueprintPure, Category = "TimeThief|Weapon")
	int32 GetMaxAmmo() const { return MaxAmmo; }

	UFUNCTION(BlueprintPure, Category = "TimeThief|Weapon")
	bool IsReloading() const { return bIsReloading; }

	UFUNCTION(BlueprintPure, Category = "TimeThief|Weapon")
	bool IsFiring() const { return bIsFiring; }

	UFUNCTION(BlueprintPure, Category = "TimeThief|Weapon")
	float GetCurrentSpread() const { return CurrentSpread; }

	UFUNCTION(BlueprintPure, Category = "TimeThief|Weapon")
	float GetSpreadAngleForFire() const { return FMath::Clamp(BaseSpread + CurrentSpread, 0.0f, MaxSpread); }

	void SetDamageBonus(float InDamageBonus) { DamageBonus = FMath::Max(0.0f, InDamageBonus); }
	void SetRecoilReduction(float InRecoilReduction) { RecoilReduction = FMath::Max(0.0f, InRecoilReduction); }
	void SetCapacityBonus(int32 InBonus);

	UFUNCTION(BlueprintPure, Category = "TimeThief|Weapon|Stats")
	float GetDamageBonus() const { return DamageBonus; }

	UFUNCTION(BlueprintPure, Category = "TimeThief|Weapon|Stats")
	float GetRecoilReduction() const { return RecoilReduction; }

	UFUNCTION(BlueprintPure, Category = "TimeThief|Weapon|Ammo")
	int32 GetCapacityBonus() const { return CapacityBonus; }

	FOnWeaponAmmoChangedSignature OnAmmoChanged_Delegate;

	UFUNCTION(BlueprintPure, Category = "Weapon")
	FGameplayTag GetWeaponTag() const { return WeaponTag; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	FName GetSocketName() const { return SocketName; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	UStaticMesh* GetWeaponMeshAsset() const { return WeaponMeshAsset; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	UStaticMeshComponent* GetWeaponMeshComponent() const;

	UFUNCTION(BlueprintPure, Category = "Weapon")
	TSubclassOf<UAnimInstance> GetEquipAnimLayer() const { return EquipAnimLayer; }

	UFUNCTION(BlueprintPure, Category = "Weapon|Animation")
	UAnimMontage* GetEquipMontage() const { return EquipMontage; }

	UFUNCTION(BlueprintPure, Category = "Weapon|Animation")
	UAnimMontage* GetUnequipMontage() const { return UnequipMontage; }

	UFUNCTION(BlueprintPure, Category = "Weapon|Socket")
	FName GetMuzzleSocketName() const { return MuzzleSocketName; }

	UFUNCTION(BlueprintPure, Category = "Weapon|Socket")
	FName GetLeftHandIKSocketName() const { return LeftHandIKSocketName; }

	UFUNCTION(BlueprintPure, Category = "Weapon|Socket")
	FTransform GetSocketTransformByName(FName InSocketName) const;

	virtual void OnEquipped();
	virtual void OnUnequipped();
	
public:
	virtual FWeaponStatData GetWeaponStatDataForNetwork() const;
	virtual void SetWeaponStatForNetwork(const FWeaponStatData& InStatData);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	virtual void PlayFireEffects();
	virtual void ExecuteFireShot();
	virtual void OnReloadStarted();
	virtual void OnReloadFinished();
	virtual void ApplyRecoilAndSpread();
	virtual uint32 GetCombatAttackShotSeed() const;
	void BroadcastCombatAttackRequest(ECombatNotifyType NotifyType) const;
	FVector GetLocalAttackOrigin() const;
	FVector GetLocalAttackDirection() const;
	bool ResolveFireAimView(FVector& OutViewLocation, FVector& OutViewDirection) const;
	void CacheLastShotSyncData(const FVector& InOrigin, const FVector& InDirection);
	bool TryGetLastShotSyncData(FVector& OutOrigin, FVector& OutDirection) const;

public:
	virtual void ExecuteRemoteFireShot();
	void SetRemoteShotSyncData(const FVector& InOrigin, const FVector& InDirection);

	void NotifyAmmoChanged();
	FVector GetMuzzleLocation() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Stats")
	float FireRate = 600.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Stats")
	float RoundsPerSecond = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Stats")
	float DamageBonus = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Stats")
	float RecoilReduction = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Ammo")
	int32 MaxAmmo = 30;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Ammo")
	float ReloadTime = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Spread")
	float MaxSpread = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Spread")
	float BaseSpread = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Spread")
	float SpreadIncreasePerShot = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Spread")
	float SpreadDecreasePerSecond = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Effects")
	TObjectPtr<USoundBase> ReloadSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Animation")
	TObjectPtr<UAnimMontage> ReloadAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Animation")
	FName WeaponAnimSlot = FName("DefaultSlot");

	UPROPERTY(VisibleInstanceOnly, Category = "TimeThief|Weapon|Runtime")
	int32 CurrentAmmo = 0;

	UPROPERTY(VisibleInstanceOnly, Category = "TimeThief|Weapon|Runtime")
	float CurrentSpread = 0.0f;

	UPROPERTY(VisibleInstanceOnly, Category = "TimeThief|Weapon|Runtime")
	int32 CapacityBonus = 0;

	UPROPERTY(VisibleInstanceOnly, Category = "TimeThief|Weapon|Runtime")
	int32 BaseMaxAmmo = 0;

	UPROPERTY(VisibleInstanceOnly, Category = "TimeThief|Weapon|Runtime")
	bool bWantsToFire = false;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Effects")
	TObjectPtr<USoundBase> FireSound;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Animation")
	TObjectPtr<UAnimMontage> FireAnimation;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Effects")
	TObjectPtr<UParticleSystem> MuzzleFlashEffect;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon")
	float AlertTime = 2;
	
	float RemainingTime = 0.0f;
	
	bool bIsFiring = false;
	bool bIsReloading = false;

	float NextAllowedFireTime = 0.0f;

	FTimerHandle AutoFireTimerHandle;
	FTimerHandle ReloadTimerHandle;

	bool bHasLastShotSyncData = false;
	FVector LastShotOrigin = FVector::ZeroVector;
	FVector LastShotDirection = FVector::ForwardVector;

	bool bHasRemoteShotSyncData = false;
	FVector RemoteShotOrigin = FVector::ZeroVector;
	FVector RemoteShotDirection = FVector::ForwardVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FGameplayTag WeaponTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FName SocketName = TEXT("WeaponSocket");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UStaticMesh> WeaponMeshAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Socket")
	FName MuzzleSocketName = TEXT("Muzzle");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Socket")
	FName LeftHandIKSocketName = TEXT("LeftHandIK");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TSubclassOf<UAnimInstance> EquipAnimLayer;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> EquipMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> UnequipMontage;
	
	void HandleReloadResult(uint32 DeltaAmmo, uint32 NewAmmo);

private:
	float GetFireInterval() const;
	void StopFiringLoop();
	void HandleAutoFireShot();
	void FinishReload();
};