#include "Weapon/TimeThiefRocketLauncher.h"
#include "Character/TimeThiefCharacterBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Weapon/TimeThiefRocketProjectile.h"

ATimeThiefRocketLauncher::ATimeThiefRocketLauncher()
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

void ATimeThiefRocketLauncher::ExecuteFireShot()
{
	if (SpawnRocketProjectile())
	{
		PlayFireEffects();
	}
}

bool ATimeThiefRocketLauncher::SpawnRocketProjectile()
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

	FVector CameraLocation = GetActorLocation();
	FVector CameraAimDirection = GetActorForwardVector();

	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController()))
		{
			FRotator CameraRotation;
			PC->GetPlayerViewPoint(CameraLocation, CameraRotation);
			CameraAimDirection = CameraRotation.Vector();
		}
		else
		{
			CameraLocation = OwnerPawn->GetPawnViewLocation();
			CameraAimDirection = OwnerPawn->GetBaseAimRotation().Vector();
		}
	}

	const FVector MuzzleLocation = GetMuzzleLocation();
	const FVector CameraTraceEnd = CameraLocation + (CameraAimDirection * AimTraceRange);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(GetOwner());
	QueryParams.bTraceComplex = true;

	FHitResult CameraHitResult;
	World->LineTraceSingleByChannel(CameraHitResult, CameraLocation, CameraTraceEnd, ECC_Visibility, QueryParams);
	const FVector TargetLocation = CameraHitResult.bBlockingHit ? CameraHitResult.ImpactPoint : CameraTraceEnd;

	const FVector ShootDirection = (TargetLocation - MuzzleLocation).GetSafeNormal();
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
		SpawnParams.Owner = GetOwner();
		SpawnParams.Instigator = Cast<APawn>(GetOwner());
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
		Projectile->InitializeProjectile(GetOwner(), Cast<APawn>(GetOwner()));
		return true;
	}

	return false;
}

void ATimeThiefRocketLauncher::PlayFireEffects()
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
		if (ATimeThiefCharacterBase* BaseCharacter = Cast<ATimeThiefCharacterBase>(GetOwner()))
		{
			BaseCharacter->PlayAnimationOnAllMeshes(FireAnimation, WeaponAnimSlot);
		}
	}
}