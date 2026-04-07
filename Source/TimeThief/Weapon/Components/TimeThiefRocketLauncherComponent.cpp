#include "Weapon/Components/TimeThiefRocketLauncherComponent.h"
#include "Weapon/TimeThiefMasterWeapon.h"
#include "Character/TimeThiefCharacterBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Weapon/TimeThiefRocketProjectile.h"

UTimeThiefRocketLauncherComponent::UTimeThiefRocketLauncherComponent()
{
	FireRate = 48.0f;
	RoundsPerSecond = 48.0f / 60.0f;
	MaxAmmo = 1;
	ReloadTime = 3.0f;
	BaseSpread = 0.0f;
	MaxSpread = 0.0f;
	SpreadIncreasePerShot = 0.0f;
	SpreadDecreasePerSecond = 0.0f;
}

void UTimeThiefRocketLauncherComponent::ExecuteFireShot()
{
	if (SpawnRocketProjectile())
	{
		PlayFireEffects();
	}
}

bool UTimeThiefRocketLauncherComponent::SpawnRocketProjectile()
{
	if (!RocketProjectileClass)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FVector CameraLocation = FVector::ZeroVector;
	FVector CameraAimDirection = FVector::ForwardVector;
	ResolveFireAimView(CameraLocation, CameraAimDirection);

	const FVector MuzzleLocation = GetMuzzleLocation();
	const FVector CameraTraceEnd = CameraLocation + (CameraAimDirection * AimTraceRange);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetOwner());
	if (GetOwner())
	{
		QueryParams.AddIgnoredActor(GetOwner()->GetOwner());
	}
	QueryParams.bTraceComplex = true;

	FHitResult CameraHitResult;
	World->LineTraceSingleByChannel(CameraHitResult, CameraLocation, CameraTraceEnd, ECC_Visibility, QueryParams);
	const FVector TargetLocation = CameraHitResult.bBlockingHit ? CameraHitResult.ImpactPoint : CameraTraceEnd;

	const FVector ShootDirection = (TargetLocation - MuzzleLocation).GetSafeNormal();
	CacheLastShotSyncData(MuzzleLocation, ShootDirection);
	const FVector SpawnLocation = MuzzleLocation + ShootDirection;
	const FTransform SpawnTransform(ShootDirection.Rotation(), SpawnLocation);

	ATimeThiefRocketProjectile* Projectile = nullptr;
	for (ATimeThiefRocketProjectile* PooledProj : ProjectilePool)
	{
		if (PooledProj && !PooledProj->IsActive())
		{
			Projectile = PooledProj;
			break;
		}
	}

	if (!Projectile)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = GetOwner() ? GetOwner()->GetOwner() : nullptr;
		SpawnParams.Instigator = Cast<APawn>(SpawnParams.Owner);
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		Projectile = World->SpawnActor<ATimeThiefRocketProjectile>(RocketProjectileClass, SpawnTransform, SpawnParams);
		if (Projectile)
		{
			ProjectilePool.Add(Projectile);
		}
	}

	if (Projectile)
	{
		Projectile->ActivateProjectile(SpawnTransform);
		Projectile->InitializeProjectile(GetOwner() ? GetOwner()->GetOwner() : nullptr, Cast<APawn>(GetOwner() ? GetOwner()->GetOwner() : nullptr));
		return true;
	}

	return false;
}

void UTimeThiefRocketLauncherComponent::PlayFireEffects()
{
	const FVector MuzzleLocation = GetMuzzleLocation();
	const FRotator MuzzleRotation = GetSocketTransformByName(GetMuzzleSocketName()).GetRotation().Rotator();

	if (MuzzleFlashEffect)
	{
		UGameplayStatics::SpawnEmitterAtLocation(this, MuzzleFlashEffect, MuzzleLocation, MuzzleRotation);
	}

	if (FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, MuzzleLocation);
	}

	if (FireAnimation)
	{
		if (AActor* MasterWeapon = GetOwner())
		{
			if (ATimeThiefCharacterBase* BaseCharacter = Cast<ATimeThiefCharacterBase>(MasterWeapon->GetOwner()))
			{
				BaseCharacter->PlayAnimationOnAllMeshes(FireAnimation, WeaponAnimSlot);
			}
		}
	}
}