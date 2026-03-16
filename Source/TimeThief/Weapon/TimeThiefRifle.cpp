#include "Weapon/TimeThiefRifle.h"
#include "Animation/Player/TimeThiefPlayerAnimInstance.h"
#include "Character/TimeThiefCharacterBase.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Particles/ParticleSystem.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"

ATimeThiefRifle::ATimeThiefRifle()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void ATimeThiefRifle::BeginPlay()
{
	Super::BeginPlay();
	
	CurrentAmmo = MaxAmmo;
	CurrentSpread = 0.0f;
}

void ATimeThiefRifle::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoFireTimerHandle);
		World->GetTimerManager().ClearTimer(ReloadTimerHandle);
	}
	
	Super::EndPlay(EndPlayReason);
}

void ATimeThiefRifle::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bIsFiring && CurrentSpread > 0.0f)
	{
		CurrentSpread = FMath::FInterpConstantTo(CurrentSpread, 0.0f, DeltaTime, SpreadDecreasePerSecond);
	}
}

void ATimeThiefRifle::StartFire()
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

	if (CanFire())
	{
		FireShot();

		if (UWorld* World = GetWorld())
		{
			const float FireInterval = 60.0f / FireRate;
			World->GetTimerManager().SetTimer(
				AutoFireTimerHandle,
				this,
				&ATimeThiefRifle::FireShot,
				FireInterval,
				true
			);
		}
	}
}

void ATimeThiefRifle::StopFire()
{
	bIsFiring = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoFireTimerHandle);
	}
}

void ATimeThiefRifle::Reload()
{
	if (bIsReloading || CurrentAmmo >= MaxAmmo)
	{
		return;
	}

	bIsReloading = true;
	StopFire();

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

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(ReloadTimerHandle, this, &ATimeThiefRifle::FinishReload, ReloadTime, false);
	}
}

void ATimeThiefRifle::FinishReload()
{
	CurrentAmmo = MaxAmmo;
	bIsReloading = false;
	OnAmmoChanged_Delegate.Broadcast(CurrentAmmo, MaxAmmo);
}

bool ATimeThiefRifle::CanFire() const
{
	return CurrentAmmo > 0 && !bIsReloading;
}

void ATimeThiefRifle::FireShot()
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
	OnAmmoChanged_Delegate.Broadcast(CurrentAmmo, MaxAmmo);

	FHitScanResult HitResult = PerformHitScan();

	if (HitResult.bHit)
	{
		ApplyDamage(HitResult);
		PlayImpactEffects(HitResult);
	}

	PlayFireEffects();
	ApplyRecoilAndSpread();

	CurrentSpread = FMath::Clamp(CurrentSpread + SpreadIncreasePerShot, 0.0f, MaxSpread);
}

FHitScanResult ATimeThiefRifle::PerformHitScan() const
{
	FHitScanResult Result;

	FVector CameraLocation = GetActorLocation();
	FVector CameraAimDir = GetActorForwardVector();

	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController()))
		{
			FRotator CameraRotation;
			PC->GetPlayerViewPoint(CameraLocation, CameraRotation);
			CameraAimDir = CameraRotation.Vector();
		}
		else
		{
			CameraLocation = OwnerPawn->GetPawnViewLocation();
			CameraAimDir = OwnerPawn->GetBaseAimRotation().Vector();
		}
	}

	if (CurrentSpread > 0.0f)
	{
		const float HalfSpread = FMath::DegreesToRadians(CurrentSpread * 0.5f);
		CameraAimDir = FMath::VRandCone(CameraAimDir, HalfSpread);
	}

	const FVector TraceEnd = CameraLocation + CameraAimDir * MaxRange;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(GetOwner());
	QueryParams.bTraceComplex = true;
	QueryParams.bReturnPhysicalMaterial = true;

	FHitResult CameraHitResult;
	GetWorld()->LineTraceSingleByChannel(CameraHitResult, CameraLocation, TraceEnd, ECC_Visibility, QueryParams);

	const FVector TargetLocation = CameraHitResult.bBlockingHit ? CameraHitResult.ImpactPoint : TraceEnd;
	const FVector MuzzleLocation = GetMuzzleLocation();

	FHitResult WeaponHitResult;
	const bool bWeaponHit = GetWorld()->LineTraceSingleByChannel(WeaponHitResult, MuzzleLocation, TargetLocation, ECC_Visibility, QueryParams);

	const FVector DebugEndLocation = bWeaponHit ? WeaponHitResult.ImpactPoint : TargetLocation;
	DrawDebugLine(GetWorld(), MuzzleLocation, DebugEndLocation, FColor::Red, false, 2.0f, 0, 1.0f);
	if (bWeaponHit)
	{
		DrawDebugPoint(GetWorld(), DebugEndLocation, 5.0f, FColor::Green, false, 2.0f);
	}

	Result.FireDirection = (TargetLocation - MuzzleLocation).GetSafeNormal();

	if (bWeaponHit)
	{
		Result.bHit = true;
		Result.HitLocation = WeaponHitResult.ImpactPoint;
		Result.HitNormal = WeaponHitResult.ImpactNormal;
		Result.HitActor = WeaponHitResult.GetActor();
		Result.HitBoneName = WeaponHitResult.BoneName;
		Result.OriginalHitResult = WeaponHitResult;
	}
	else if (CameraHitResult.bBlockingHit)
	{
		Result.bHit = true;
		Result.HitLocation = CameraHitResult.ImpactPoint;
		Result.HitNormal = CameraHitResult.ImpactNormal;
		Result.HitActor = CameraHitResult.GetActor();
		Result.HitBoneName = CameraHitResult.BoneName;
		Result.OriginalHitResult = CameraHitResult;
	}

	return Result;
}

void ATimeThiefRifle::ApplyDamage(const FHitScanResult& HitResult)
{
	if (!HitResult.HitActor.IsValid())
	{
		return;
	}

	AController* InstigatorController = nullptr;
	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		InstigatorController = OwnerPawn->GetController();
	}

	UGameplayStatics::ApplyPointDamage(
		HitResult.HitActor.Get(),
		BaseDamage,
		HitResult.FireDirection,
		HitResult.OriginalHitResult,
		InstigatorController,
		this,
		nullptr
	);
}

void ATimeThiefRifle::PlayFireEffects()
{
	const FVector MuzzleLoc = GetMuzzleLocation();

	if (MuzzleFlashEffect)
	{
		UGameplayStatics::SpawnEmitterAtLocation(
			this,
			MuzzleFlashEffect,
			MuzzleLoc,
			GetActorRotation()
		);
	}

	if (FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, MuzzleLoc);
	}

	if (FireAnimation)
	{
		if (ATimeThiefCharacterBase* BaseChar = Cast<ATimeThiefCharacterBase>(GetOwner()))
		{
			BaseChar->PlayAnimationOnAllMeshes(FireAnimation, WeaponAnimSlot);
		}
	}
}

void ATimeThiefRifle::PlayImpactEffects(const FHitScanResult& HitResult)
{
	if (ImpactEffect && HitResult.bHit)
	{
		const FVector IncomingDir = HitResult.FireDirection.GetSafeNormal();
		const FVector ReflectDir = FMath::GetReflectionVector(IncomingDir, HitResult.HitNormal);
		
		const FRotator ImpactRotation = FRotationMatrix::MakeFromXZ(ReflectDir, HitResult.HitNormal).Rotator();

		UGameplayStatics::SpawnEmitterAtLocation(
			this,
			ImpactEffect,
			HitResult.HitLocation,
			ImpactRotation
		);
	}
}

FVector ATimeThiefRifle::GetMuzzleLocation() const
{
	if (WeaponMesh && WeaponMesh->DoesSocketExist(GetMuzzleSocketName()))
	{
		return WeaponMesh->GetSocketLocation(GetMuzzleSocketName());
	}
	return GetActorLocation();
}

FVector ATimeThiefRifle::GetAimDirection() const
{
	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController()))
		{
			FVector CameraLocation;
			FRotator CameraRotation;
			PC->GetPlayerViewPoint(CameraLocation, CameraRotation);
			return CameraRotation.Vector();
		}
		return OwnerPawn->GetControlRotation().Vector();
	}
	return GetActorForwardVector();
}

void ATimeThiefRifle::ApplyRecoilAndSpread()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		return;
	}

	if (ACharacter* OwnerChar = Cast<ACharacter>(OwnerPawn))
	{
		if (UTimeThiefPlayerAnimInstance* AnimInst = Cast<UTimeThiefPlayerAnimInstance>(OwnerChar->GetMesh()->GetAnimInstance()))
		{
			AnimInst->SetRecoilRecoverySpeed(RecoilRecoverySpeed, SpreadDecreasePerSecond);
			const FVector2D RecoilDelta = AnimInst->ApplyFireSpread(MaxVerticalRecoil, MaxHorizontalRecoil, RecoilBuildupPerShot, SpreadIncreasePerShot);

			if (APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController()))
			{
				PC->AddPitchInput(-RecoilDelta.Y);
				PC->AddYawInput(RecoilDelta.X);
			}
		}
	}
}