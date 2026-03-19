#pragma once

#include "CoreMinimal.h"
#include "Weapon/TimeThiefWeaponBase.h"
#include "TimeThiefShotgun.generated.h"

class USoundBase;
class UParticleSystem;
class UAnimSequenceBase;

USTRUCT(BlueprintType)
struct FShotgunHitResult
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
	FVector FireDirection = FVector::ForwardVector;

	FHitResult OriginalHitResult;
};

UCLASS()
class TIMETHIEF_API ATimeThiefShotgun : public ATimeThiefWeaponBase
{
	GENERATED_BODY()

public:
	ATimeThiefShotgun();

protected:
	virtual void BeginPlay() override;
	virtual void ExecuteFireShot() override;
	TArray<FShotgunHitResult> PerformPelletHitScan() const;
	void ApplyDamage(const TArray<FShotgunHitResult>& HitResults);
	void PlayFireEffects();
	void PlayImpactEffects(const TArray<FShotgunHitResult>& HitResults);
	virtual void ApplyRecoilAndSpread() override;


	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Stats")
	float DamagePerPellet = 12.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Stats")
	float MaxRange = 3000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Stats")
	int32 PelletCount = 12;


	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Effects")
	TObjectPtr<UParticleSystem> MuzzleFlashEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Effects")
	TObjectPtr<UParticleSystem> ImpactEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Effects")
	TObjectPtr<USoundBase> FireSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Animation")
	TObjectPtr<UAnimSequenceBase> FireAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Recoil")
	float VerticalRecoil = 6.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Recoil")
	float HorizontalRecoil = 3.0f;
};


