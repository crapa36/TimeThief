#include "Weapon/TimeThiefRifle.h"
#include "Animation/Player/TimeThiefPlayerAnimInstance.h"
#include "Character/TimeThiefCharacterBase.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

ATimeThiefRifle::ATimeThiefRifle()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ATimeThiefRifle::BeginPlay()
{
	Super::BeginPlay();

	CurrentAmmo = MaxAmmo;
}

void ATimeThiefRifle::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(AutoFireTimerHandle);
	GetWorldTimerManager().ClearTimer(ReloadTimerHandle);
	
	Super::EndPlay(EndPlayReason);
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

		const float FireInterval = 60.0f / FireRate;
		GetWorldTimerManager().SetTimer(
			AutoFireTimerHandle,
			this,
			&ATimeThiefRifle::FireShot,
			FireInterval,
			true
		);
	}
}

void ATimeThiefRifle::StopFire()
{
	bIsFiring = false;
	GetWorldTimerManager().ClearTimer(AutoFireTimerHandle);
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

	GetWorldTimerManager().SetTimer(ReloadTimerHandle, this, &ATimeThiefRifle::FinishReload, ReloadTime, false);
}

void ATimeThiefRifle::FinishReload()
{
	CurrentAmmo = MaxAmmo;
	bIsReloading = false;
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

	FHitScanResult HitResult = PerformHitScan();

	if (HitResult.bHit)
	{
		ApplyDamage(HitResult);
		PlayImpactEffects(HitResult);
	}

	PlayFireEffects();
	ApplyRecoil();
}

FHitScanResult ATimeThiefRifle::PerformHitScan() const
{
	FHitScanResult Result;

	FVector StartLocation = GetMuzzleLocation();
	FVector AimDir = GetAimDirection();

	float CurrentSpread = SpreadAngle;
	if (ACharacter* OwnerChar = Cast<ACharacter>(GetOwner()))
	{
		if (UTimeThiefPlayerAnimInstance* AnimInst = Cast<UTimeThiefPlayerAnimInstance>(OwnerChar->GetMesh()->GetAnimInstance()))
		{
			CurrentSpread += AnimInst->GetCurrentSpreadAngle();
		}
	}

	if (CurrentSpread > 0.0f)
	{
		const float HalfSpread = FMath::DegreesToRadians(CurrentSpread * 0.5f);
		AimDir = FMath::VRandCone(AimDir, HalfSpread);
	}

	Result.FireDirection = AimDir;

	FVector EndLocation = StartLocation + AimDir * MaxRange;

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(GetOwner());
	QueryParams.bTraceComplex = true;
	QueryParams.bReturnPhysicalMaterial = true;

	bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult,
		StartLocation,
		EndLocation,
		ECC_Visibility,
		QueryParams
	);

	if (bHit)
	{
		Result.bHit = true;
		Result.HitLocation = HitResult.ImpactPoint;
		Result.HitNormal = HitResult.ImpactNormal;
		Result.HitActor = HitResult.GetActor();
		Result.HitBoneName = HitResult.BoneName;
		Result.OriginalHitResult = HitResult;

#if ENABLE_DRAW_DEBUG
		DrawDebugLine(GetWorld(), StartLocation, HitResult.ImpactPoint, FColor::Red, false, 1.0f, 0, 1.0f);
#endif
	}
	else
	{
#if ENABLE_DRAW_DEBUG
		DrawDebugLine(GetWorld(), StartLocation, EndLocation, FColor::Yellow, false, 1.0f, 0, 1.0f);
#endif
	}

	return Result;
}

void ATimeThiefRifle::ApplyDamage(const FHitScanResult& HitResult)
{
	if (!HitResult.HitActor.IsValid())
	{
		return;
	}

	float FinalDamage = BaseDamage;

	AController* InstigatorController = nullptr;
	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		InstigatorController = OwnerPawn->GetController();
	}

	UGameplayStatics::ApplyPointDamage(
		HitResult.HitActor.Get(),
		FinalDamage,
		HitResult.FireDirection,
		HitResult.OriginalHitResult,
		InstigatorController,
		this,
		nullptr
	);
}

void ATimeThiefRifle::PlayFireEffects()
{
	FVector MuzzleLoc = GetMuzzleLocation();

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

void ATimeThiefRifle::ApplyRecoil()
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
			AnimInst->SetRecoilRecoverySpeed(RecoilRecoverySpeed, SpreadRecoverySpeed);
			const FVector2D RecoilDelta = AnimInst->ApplyFireSpread(MaxVerticalRecoil, MaxHorizontalRecoil, RecoilBuildupPerShot, SpreadBuildupPerShot);

			APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
			if (PC)
			{
				PC->AddPitchInput(-RecoilDelta.Y);
				PC->AddYawInput(RecoilDelta.X);
			}
		}
	}
}

