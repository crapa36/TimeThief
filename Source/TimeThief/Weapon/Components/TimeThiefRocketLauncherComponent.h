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

protected:
	virtual void ExecuteFireShot() override;

	void PlayFireEffects();
	bool SpawnRocketProjectile();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Projectile")
	TSubclassOf<ATimeThiefRocketProjectile> RocketProjectileClass;

	UPROPERTY()
	TArray<TObjectPtr<ATimeThiefRocketProjectile>> ProjectilePool;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Projectile")
	float AimTraceRange = 50000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Effects")
	TObjectPtr<UParticleSystem> MuzzleFlashEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Effects")
	TObjectPtr<USoundBase> FireSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Animation")
	TObjectPtr<UAnimSequenceBase> FireAnimation;
};