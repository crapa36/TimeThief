#include "Weapon/TimeThiefWeaponBase.h"
#include "Components/StaticMeshComponent.h"
#include "Character/TimeThiefCharacterBase.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Animation/AnimSequenceBase.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

ATimeThiefWeaponBase::ATimeThiefWeaponBase() {
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	SetRootComponent(WeaponMesh);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	WeaponMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
}

void ATimeThiefWeaponBase::BeginPlay()
{
	Super::BeginPlay();
	CurrentAmmo = MaxAmmo;
	CurrentSpread = 0.0f;
	NextAllowedFireTime = 0.0f;
	bWantsToFire = false;
}

void ATimeThiefWeaponBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoFireTimerHandle);
		World->GetTimerManager().ClearTimer(ReloadTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void ATimeThiefWeaponBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ATimeThiefWeaponBase::StartFire()
{
	bWantsToFire = true;

	if (bIsReloading || bIsFiring)
	{
		return;
	}

	if (CurrentAmmo <= 0)
	{
		Reload();
		return;
	}

	bIsFiring = true;

	if (UWorld* World = GetWorld())
	{
		const float CurrentTime = World->GetTimeSeconds();
		const float Delay = FMath::Max(0.0f, NextAllowedFireTime - CurrentTime);

		if (Delay <= KINDA_SMALL_NUMBER)
		{
			HandleAutoFireShot();
		}
		else
		{
			World->GetTimerManager().SetTimer(AutoFireTimerHandle, this, &ATimeThiefWeaponBase::HandleAutoFireShot, Delay, false);
		}
	}
}

void ATimeThiefWeaponBase::StopFire()
{
	bWantsToFire = false;
	StopFiringLoop();
}

void ATimeThiefWeaponBase::Reload()
{
	if (!CanReload())
	{
		return;
	}

	bIsReloading = true;
	StopFiringLoop();
	OnReloadStarted();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(ReloadTimerHandle, this, &ATimeThiefWeaponBase::FinishReload, ReloadTime, false);
	}
}

bool ATimeThiefWeaponBase::CanFire() const
{
	return CurrentAmmo > 0 && !bIsReloading;
}

bool ATimeThiefWeaponBase::CanReload() const
{
	return !bIsReloading && CurrentAmmo < MaxAmmo;
}

void ATimeThiefWeaponBase::ExecuteFireShot()
{
}

void ATimeThiefWeaponBase::OnReloadStarted()
{
	if (ReloadAnimation)
	{
		if (ATimeThiefCharacterBase* BaseChar = Cast<ATimeThiefCharacterBase>(GetOwner()))
		{
			BaseChar->PlayAnimationOnAllMeshes(ReloadAnimation, WeaponAnimSlot);
		}
	}

	if (ReloadSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ReloadSound, GetActorLocation());
	}
}

void ATimeThiefWeaponBase::OnReloadFinished()
{
}

void ATimeThiefWeaponBase::ApplyRecoilAndSpread()
{
}

void ATimeThiefWeaponBase::NotifyAmmoChanged()
{
	OnAmmoChanged_Delegate.Broadcast(CurrentAmmo, MaxAmmo);
}

FVector ATimeThiefWeaponBase::GetMuzzleLocation() const
{
	if (WeaponMesh && WeaponMesh->DoesSocketExist(MuzzleSocketName))
	{
		return WeaponMesh->GetSocketLocation(MuzzleSocketName);
	}
	return GetActorLocation();
}

void ATimeThiefWeaponBase::HandleAutoFireShot()
{
	if (!bIsFiring || !CanFire())
	{
		StopFiringLoop();
		if (CurrentAmmo <= 0)
		{
			Reload();
		}
		return;
	}

	CurrentAmmo--;
	NotifyAmmoChanged();

	ExecuteFireShot();
	ApplyRecoilAndSpread();

	if (UWorld* World = GetWorld())
	{
		NextAllowedFireTime = World->GetTimeSeconds() + GetFireInterval();
	}

	if (CurrentAmmo <= 0)
	{
		StopFiringLoop();
		Reload();
	}
	else if (bWantsToFire)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(AutoFireTimerHandle, this, &ATimeThiefWeaponBase::HandleAutoFireShot, GetFireInterval(), false);
		}
	}
	else
	{
		StopFiringLoop();
	}
}

void ATimeThiefWeaponBase::StopFiringLoop()
{
	bIsFiring = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoFireTimerHandle);
	}
}

float ATimeThiefWeaponBase::GetFireInterval() const
{
	if (RoundsPerSecond > KINDA_SMALL_NUMBER)
	{
		return 1.0f / RoundsPerSecond;
	}

	return FireRate > 0.0f ? (60.0f / FireRate) : 0.1f;
}

void ATimeThiefWeaponBase::FinishReload()
{
	CurrentAmmo = MaxAmmo;
	bIsReloading = false;
	NotifyAmmoChanged();
	OnReloadFinished();

	if (bWantsToFire)
	{
		StartFire();
	}
}

FTransform ATimeThiefWeaponBase::GetSocketTransformByName(FName InSocketName) const
{
	if (WeaponMesh && WeaponMesh->DoesSocketExist(InSocketName))
	{
		return WeaponMesh->GetSocketTransform(InSocketName);
	}
	return GetActorTransform();
}