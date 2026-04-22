#include "Weapon/Components/TimeThiefRocketLauncherComponent.h"
#include "Character/TimeThiefCharacterBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Weapon/TimeThiefRocketProjectile.h"
#include "Utils/TimeThiefAimStatics.h"

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

void UTimeThiefRocketLauncherComponent::ExecuteRemoteFireShot()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ExecuteFireShot();
		return;
	}

	PlayFireEffects();
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
	FVector CameraTraceEnd = CameraLocation + (CameraAimDirection * AimTraceRange);

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Reserve(2);
	ActorsToIgnore.Add(GetOwner());
	if (GetOwner())
	{
		ActorsToIgnore.Add(GetOwner()->GetParentActor());
	}

	FHitResult CameraHitResult;
	UTimeThiefAimStatics::TraceFromView(
		World,
		CameraLocation,
		CameraAimDirection,
		AimTraceRange,
		ActorsToIgnore,
		CameraHitResult,
		CameraTraceEnd,
		ECC_Visibility,
		true,
		false);
	const FVector TargetLocation = CameraHitResult.bBlockingHit ? CameraHitResult.ImpactPoint : CameraTraceEnd;

	const FVector ShootDirection = (TargetLocation - MuzzleLocation).GetSafeNormal();
	CacheLastShotSyncData(MuzzleLocation, ShootDirection);
	const FVector SpawnLocation = MuzzleLocation + ShootDirection;
	const FTransform SpawnTransform(ShootDirection.Rotation(), SpawnLocation);

	AActor* ShooterActor = GetOwner() ? GetOwner()->GetParentActor() : nullptr;
	APawn* ShooterPawn = Cast<APawn>(ShooterActor);

	ATimeThiefRocketProjectile* Projectile = nullptr;
	const int32 PoolCount = ProjectilePool.Num();
	if (PoolCount > 0)
	{
		const int32 StartIndex = FMath::Clamp(NextPoolIndex, 0, PoolCount - 1);
		for (int32 Offset = 0; Offset < PoolCount; ++Offset)
		{
			const int32 CandidateIndex = (StartIndex + Offset) % PoolCount;
			ATimeThiefRocketProjectile* Candidate = ProjectilePool[CandidateIndex];
			if (!IsValid(Candidate) || Candidate->IsActorBeingDestroyed() || Candidate->IsActive())
			{
				continue;
			}

			Projectile = Candidate;
			NextPoolIndex = (CandidateIndex + 1) % PoolCount;
			break;
		}
	}

	if (!Projectile)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = ShooterActor;
		SpawnParams.Instigator = ShooterPawn;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		Projectile = World->SpawnActor<ATimeThiefRocketProjectile>(RocketProjectileClass, SpawnTransform, SpawnParams);
		if (Projectile)
		{
			ProjectilePool.Add(Projectile);
			NextPoolIndex = 0;
		}
	}

	if (Projectile)
	{
		Projectile->SetDamageBonus(GetDamageBonus());

#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Log, TEXT("[RocketLauncher][Fire] DamageBonus=%.2f RecoilReduction=%.3f"), GetDamageBonus(), GetRecoilReduction());
#endif

		// Ensure ownership/ignore setup is valid before collision and movement start.
		Projectile->InitializeProjectile(ShooterActor, ShooterPawn);
		Projectile->ActivateProjectile(SpawnTransform);
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