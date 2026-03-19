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

	if (!bIsFiring && CurrentSpread > 0.0f)
	{
		CurrentSpread = FMath::FInterpConstantTo(CurrentSpread, 0.0f, DeltaTime, SpreadDecreasePerSecond);
	}
}

void ATimeThiefWeaponBase::StartFire()
{
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
	HandleAutoFireShot();

	if (UWorld* World = GetWorld())
	{
		const float FireInterval = FireRate > 0.0f ? (60.0f / FireRate) : 0.1f;
		World->GetTimerManager().SetTimer(AutoFireTimerHandle, this, &ATimeThiefWeaponBase::HandleAutoFireShot, FireInterval, true);
	}
}

void ATimeThiefWeaponBase::StopFire()
{
	bIsFiring = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoFireTimerHandle);
	}
}

void ATimeThiefWeaponBase::Reload()
{
	if (!CanReload())
	{
		return;
	}

	bIsReloading = true;
	StopFire();
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
	if (!CanFire())
	{
		StopFire();
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

	CurrentSpread = FMath::Clamp(CurrentSpread + SpreadIncreasePerShot, 0.0f, MaxSpread);
}

void ATimeThiefWeaponBase::FinishReload()
{
	CurrentAmmo = MaxAmmo;
	bIsReloading = false;
	NotifyAmmoChanged();
	OnReloadFinished();
}

FTransform ATimeThiefWeaponBase::GetSocketTransformByName(FName InSocketName) const
{
	if (WeaponMesh && WeaponMesh->DoesSocketExist(InSocketName))
	{
		return WeaponMesh->GetSocketTransform(InSocketName);
	}
	return GetActorTransform();
}
