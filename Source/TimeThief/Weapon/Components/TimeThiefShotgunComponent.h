#pragma once

#include "CoreMinimal.h"
#include "Weapon/Components/TimeThiefWeaponComponentBase.h"
#include "TimeThiefShotgunComponent.generated.h"

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

UCLASS(Blueprintable, ClassGroup=(TimeThief), meta=(BlueprintSpawnableComponent))
class TIMETHIEF_API UTimeThiefShotgunComponent : public UTimeThiefWeaponComponentBase
{
	GENERATED_BODY()

public:
	UTimeThiefShotgunComponent();

protected:
	virtual void BeginPlay() override;
	virtual void ExecuteFireShot() override;
	virtual uint32 GetCombatAttackShotSeed() const override;
	virtual void SetRemoteShotSeed(uint32 InShotSeed) override;
	TArray<FShotgunHitResult> PerformPelletHitScan();
	void ApplyDamage(const TArray<FShotgunHitResult>& HitResults);

	void PlayImpactEffects(const TArray<FShotgunHitResult>& HitResults);
	virtual void ApplyRecoilAndSpread() override;
	
public:
	virtual FWeaponStatData GetWeaponStatDataForNetwork() const override;
	virtual void SetWeaponStatForNetwork(const FWeaponStatData& InStatData) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Stats")
	float DamagePerPellet = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Stats")
	float MaxRange = 3000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Stats")
	int32 PelletCount = 12;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Effects")
	TObjectPtr<UParticleSystem> ImpactEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Recoil")
	float VerticalRecoil = 6.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Recoil")
	float HorizontalRecoil = 3.0f;

	uint32 LastShotSeed = 0;
	uint32 RemoteShotSeed = 0;
	bool bHasRemoteShotSeed = false;
};
