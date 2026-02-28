#pragma once

#include "CoreMinimal.h"
#include "Weapon/TimeThiefWeaponBase.h"
#include "TimeThiefRifle.generated.h"

class USoundCue;
class UParticleSystem;

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

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Weapon")
	bool CanFire() const;

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Weapon")
	int32 GetCurrentAmmo() const { return CurrentAmmo; }

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Weapon")
	int32 GetMaxAmmo() const { return MaxAmmo; }

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Weapon")
	int32 GetReserveAmmo() const { return ReserveAmmo; }

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Weapon")
	bool IsReloading() const { return bIsReloading; }

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Weapon")
	bool IsFiring() const { return bIsFiring; }

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
	float HeadshotMultiplier = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Stats")
	float FireRate = 600.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Stats")
	float MaxRange = 10000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Stats")
	float SpreadAngle = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Ammo")
	int32 MaxAmmo = 30;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Ammo")
	int32 MaxReserveAmmo = 120;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Ammo")
	float ReloadTime = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Effects")
	TObjectPtr<UParticleSystem> MuzzleFlashEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Effects")
	TObjectPtr<UParticleSystem> ImpactEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Effects")
	TObjectPtr<USoundCue> FireSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Effects")
	TObjectPtr<USoundCue> ReloadSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Effects")
	FName MuzzleSocketName = TEXT("Muzzle");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Stats")
	FName HeadshotBoneName = TEXT("head");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Animation")
	TObjectPtr<UAnimMontage> FireMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Animation")
	TObjectPtr<UAnimMontage> ReloadMontage;

	void ApplyRecoil();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Stats")
	float RecoilInputScale = 0.1f;

private:
	int32 CurrentAmmo;
	int32 ReserveAmmo;
	bool bIsFiring = false;
	bool bIsReloading = false;
	FTimerHandle AutoFireTimerHandle;
	FTimerHandle ReloadTimerHandle;
};
