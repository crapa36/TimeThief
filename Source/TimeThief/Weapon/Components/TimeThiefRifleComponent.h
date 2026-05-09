#pragma once

#include "CoreMinimal.h"
#include "Weapon/Components/TimeThiefWeaponComponentBase.h"
#include "TimeThiefRifleComponent.generated.h"

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

UCLASS(Blueprintable, ClassGroup=(TimeThief), meta=(BlueprintSpawnableComponent))
class TIMETHIEF_API UTimeThiefRifleComponent : public UTimeThiefWeaponComponentBase
{
	GENERATED_BODY()

public:
	UTimeThiefRifleComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void ExecuteFireShot() override;
	FRifleHitResult PerformHitScan();
	void ApplyDamage(const FRifleHitResult& HitResult);

	void PlayImpactEffects(const FRifleHitResult& HitResult);
	virtual void ApplyRecoilAndSpread() override;
	
public:
	virtual void SetWeaponStatForNetwork(const FWeaponStatData& InStatData) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Stats")
	float BaseDamage = 25.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Stats")
	float MaxRange = 10000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Effects")
	TObjectPtr<UParticleSystem> ImpactEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Recoil")
	float MaxVerticalRecoil = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Recoil")
	float MaxHorizontalRecoil = 0.6f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Recoil")
	float RecoilRecoverySpeed = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Recoil")
	float RecoilBuildupPerShot = 0.12f;
};