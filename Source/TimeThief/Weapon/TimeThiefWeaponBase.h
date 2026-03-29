#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "TimeThiefWeaponBase.generated.h"

class UAnimInstance;
class UAnimMontage;
class UAnimSequenceBase;
class USoundBase;
class UStaticMeshComponent;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnWeaponAmmoChangedSignature, int32, int32);

UCLASS()
class TIMETHIEF_API ATimeThiefWeaponBase : public AActor {
	GENERATED_BODY()

public:
	ATimeThiefWeaponBase();

	virtual void Tick(float DeltaTime) override;

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

	FOnWeaponAmmoChangedSignature OnAmmoChanged_Delegate;

	UFUNCTION(BlueprintPure, Category = "Weapon")
	FGameplayTag GetWeaponTag() const { return WeaponTag; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	FName GetSocketName() const { return SocketName; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	UStaticMeshComponent* GetWeaponMesh() const { return WeaponMesh; }

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


protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void ExecuteFireShot();
	virtual void OnReloadStarted();
	virtual void OnReloadFinished();
	virtual void ApplyRecoilAndSpread();

	void NotifyAmmoChanged();
	FVector GetMuzzleLocation() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Stats")
	float FireRate = 600.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Stats", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float RoundsPerSecond = 0.0f;

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
	TObjectPtr<UAnimSequenceBase> ReloadAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Animation")
	FName WeaponAnimSlot = FName("DefaultSlot");

	UPROPERTY(VisibleInstanceOnly, Category = "TimeThief|Weapon|Runtime")
	int32 CurrentAmmo = 0;

	UPROPERTY(VisibleInstanceOnly, Category = "TimeThief|Weapon|Runtime")
	float CurrentSpread = 0.0f;

	UPROPERTY(VisibleInstanceOnly, Category = "TimeThief|Weapon|Runtime")
	bool bWantsToFire = false;

	bool bIsFiring = false;
	bool bIsReloading = false;

	float NextAllowedFireTime = 0.0f;

	FTimerHandle AutoFireTimerHandle;
	FTimerHandle ReloadTimerHandle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FGameplayTag WeaponTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FName SocketName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UStaticMeshComponent> WeaponMesh;

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

private:
	float GetFireInterval() const;
	void StopFiringLoop();
	void HandleAutoFireShot();
	void FinishReload();
};