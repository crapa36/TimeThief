#pragma once

#include "CoreMinimal.h"
#include "Weapon/TimeThiefWeaponBase.h"
#include "TimeThiefRifle.generated.h"

class USoundBase;
class UParticleSystem;
class UAnimSequenceBase;

USTRUCT(BlueprintType)
struct FRifleHitResult
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
	virtual void Tick(float DeltaTime) override;

protected:
	virtual void ExecuteFireShot() override;
	FRifleHitResult PerformHitScan() const;
	void ApplyDamage(const FRifleHitResult& HitResult);
	void PlayFireEffects();
	void PlayImpactEffects(const FRifleHitResult& HitResult);
	virtual void ApplyRecoilAndSpread() override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Stats")
	float BaseDamage = 25.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Stats")
	float MaxRange = 10000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Effects")
	TObjectPtr<UParticleSystem> MuzzleFlashEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Effects")
	TObjectPtr<UParticleSystem> ImpactEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Effects")
	TObjectPtr<USoundBase> FireSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Animation")
	TObjectPtr<UAnimSequenceBase> FireAnimation;

	// 최대 수직 반동 (각도)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Recoil")
	float MaxVerticalRecoil = 1.5f;

	// 최대 수평 반동 (각도)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Recoil")
	float MaxHorizontalRecoil = 0.6f;

	// 반동 회복 속도 (각도/초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Recoil")
	float RecoilRecoverySpeed = 5.0f;

	// 1발당 반동 증가량 (각도)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Recoil")
	float RecoilBuildupPerShot = 0.12f;

};