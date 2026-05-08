#pragma once

#include "CoreMinimal.h"
#include "Weapon/Components/TimeThiefWeaponComponentBase.h"
#include "TimeThiefRocketLauncherComponent.generated.h"

class ATimeThiefRocketProjectile;
class UAnimSequenceBase;
class UParticleSystem;
class USoundBase;

UCLASS(Blueprintable, ClassGroup=(TimeThief), meta=(BlueprintSpawnableComponent))
class TIMETHIEF_API UTimeThiefRocketLauncherComponent : public UTimeThiefWeaponComponentBase
{
	GENERATED_BODY()

public:
	UTimeThiefRocketLauncherComponent();
	
	float GetProjectileSpeed() const { return ProjectileSpeed; }
	float GetExplosionRadius() const { return ExplosionRadius; }

protected:
	virtual void ExecuteFireShot() override;
	virtual void ExecuteRemoteFireShot() override;

	void PlayFireEffects();
	bool SpawnRocketProjectile();
	
public:
	virtual FWeaponStatData GetWeaponStatDataForNetwork() const override;
	virtual void SetWeaponStatForNetwork(const FWeaponStatData& InStatData) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Projectile")
	TSubclassOf<ATimeThiefRocketProjectile> RocketProjectileClass;

	UPROPERTY()
	TArray<TObjectPtr<ATimeThiefRocketProjectile>> ProjectilePool;

	int32 NextPoolIndex = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Projectile")
	float AimTraceRange = 50000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Effects")
	TObjectPtr<UParticleSystem> MuzzleFlashEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Effects")
	TObjectPtr<USoundBase> FireSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Animation")
	TObjectPtr<UAnimSequenceBase> FireAnimation;
	
	UPROPERTY()
	float ProjectileSpeed = 3000.0f;
	
	UPROPERTY()
	float ExplosionRadius = 300.0f;
};