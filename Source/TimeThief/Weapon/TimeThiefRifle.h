#pragma once

#include "CoreMinimal.h"
#include "Weapon/TimeThiefWeaponBase.h"
#include "TimeThiefRifle.generated.h"

class USoundBase;
class UParticleSystem;
class UAnimSequenceBase;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAmmoChangedSignature, int32, int32);

USTRUCT(BlueprintType)
struct FHitScanResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	bool bHit = false;

	UPROPERTY(BlueprintReadOnly)
	FVector HitLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly)
	FVector HitNormal = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly)
	TWeakObjectPtr<AActor> HitActor = nullptr;

	UPROPERTY(BlueprintReadOnly)
	FName HitBoneName = NAME_None;

	UPROPERTY(BlueprintReadOnly)
	FVector FireDirection = FVector::ForwardVector;

	FHitResult OriginalHitResult;
};

UCLASS()
class TIMETHIEF_API ATimeThiefRifle : public ATimeThiefWeaponBase
{
	GENERATED_BODY()

public:
	ATimeThiefRifle();

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Weapon")
	void StartFire();

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Weapon")
	void StopFire();

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Weapon")
	void Reload();

	UFUNCTION(BlueprintPure, Category = "TimeThief|Weapon")
	bool CanFire() const;

	UFUNCTION(BlueprintPure, Category = "TimeThief|Weapon")
	int32 GetCurrentAmmo() const { return CurrentAmmo; }

	UFUNCTION(BlueprintPure, Category = "TimeThief|Weapon")
	int32 GetMaxAmmo() const { return MaxAmmo; }

	UFUNCTION(BlueprintPure, Category = "TimeThief|Weapon")
	bool IsReloading() const { return bIsReloading; }

	UFUNCTION(BlueprintPure, Category = "TimeThief|Weapon")
	bool IsFiring() const { return bIsFiring; }

	FOnAmmoChangedSignature OnAmmoChanged_Delegate;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void FireShot();
	FHitScanResult PerformHitScan() const;
	void ApplyDamage(const FHitScanResult& HitResult);
	void PlayFireEffects();
	void PlayImpactEffects(const FHitScanResult& HitResult);
	void FinishReload();

	FVector GetMuzzleLocation() const;
	FVector GetAimDirection() const;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Stats")
	float BaseDamage = 25.0f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Stats")
	float FireRate = 600.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Stats")
	float MaxRange = 10000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Stats")
	float SpreadAngle = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Ammo")
	int32 MaxAmmo = 30;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Ammo")
	float ReloadTime = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Effects")
	TObjectPtr<UParticleSystem> MuzzleFlashEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Effects")
	TObjectPtr<UParticleSystem> ImpactEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Effects")
	TObjectPtr<USoundBase> FireSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Effects")
	TObjectPtr<USoundBase> ReloadSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Animation")
	TObjectPtr<UAnimSequenceBase> FireAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Animation")
	TObjectPtr<UAnimSequenceBase> ReloadAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Animation")
	FName WeaponAnimSlot = FName("DefaultSlot");

	void ApplyRecoil();

	// 연사 시 도달하는 최대 상하 반동 (도 단위, Pitch)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Recoil")
	float MaxVerticalRecoil = 1.5f;

	// 연사 시 도달하는 최대 좌우 반동 (도 단위, Yaw)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Recoil")
	float MaxHorizontalRecoil = 0.6f;

	// 반동 복구 속도 (클수록 빠르게 복귀)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Recoil")
	float RecoilRecoverySpeed = 5.0f;

	// 탄퍼짐 복구 속도 (클수록 빠르게 복귀)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Recoil")
	float SpreadRecoverySpeed = 2.0f;

	// 발사당 반동 증가량 (0~1 비율, 누적되어 최대 1.0까지)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Recoil")
	float RecoilBuildupPerShot = 0.12f;

	// 발사당 탄퍼짐 증가량 (0~1 비율, 누적되어 최대 1.0까지)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Recoil")
	float SpreadBuildupPerShot = 0.15f;


	private:
	UPROPERTY(VisibleInstanceOnly, Category = "TimeThief|Weapon|Runtime")
	int32 CurrentAmmo = 0;

	bool bIsFiring = false;
	bool bIsReloading = false;
	FTimerHandle AutoFireTimerHandle;
	FTimerHandle ReloadTimerHandle;
};