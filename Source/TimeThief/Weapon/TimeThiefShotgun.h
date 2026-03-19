#pragma once

#include "CoreMinimal.h"
#include "Weapon/TimeThiefWeaponBase.h"
#include "TimeThiefShotgun.generated.h"

class USoundBase;
class UParticleSystem;
class UAnimSequenceBase;

UCLASS()
class TIMETHIEF_API ATimeThiefShotgun : public ATimeThiefWeaponBase
{
	GENERATED_BODY()

public:
	ATimeThiefShotgun();

protected:
	virtual void ExecuteFireShot() override;
	virtual void ApplyRecoilAndSpread() override;

	void ApplyShotgunDamage(const FHitResult& HitResult, const FVector& FireDirection);
	void PlayFireEffects(const FVector& MuzzleLocation);
	void PlayImpactEffects(const FHitResult& HitResult, const FVector& FireDirection);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Stats")
	float DamagePerPellet = 12.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Stats")
	float MaxRange = 8000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Stats")
	int32 PelletCount = 8;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Spread")
	float PelletSpreadAngle = 3.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Effects")
	TObjectPtr<UParticleSystem> MuzzleFlashEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Effects")
	TObjectPtr<UParticleSystem> ImpactEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Effects")
	TObjectPtr<USoundBase> FireSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Animation")
	TObjectPtr<UAnimSequenceBase> FireAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Recoil")
	float MaxVerticalRecoil = 2.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Recoil")
	float MaxHorizontalRecoil = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Recoil")
	float RecoilRecoverySpeed = 4.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Recoil")
	float RecoilBuildupPerShot = 0.2f;
};


