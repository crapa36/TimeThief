#include "Weapon/TimeThiefShotgun.h"

#include "Animation/Player/TimeThiefPlayerAnimInstance.h"
#include "Character/TimeThiefCharacterBase.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"
#include "DrawDebugHelpers.h"

ATimeThiefShotgun::ATimeThiefShotgun()
{
	FireRate = 110.0f;
	MaxAmmo = 8;
	ReloadTime = 2.0f;
	MaxSpread = 7.0f;
	SpreadIncreasePerShot = 1.3f;
	SpreadDecreasePerSecond = 8.0f;
}

void ATimeThiefShotgun::ExecuteFireShot()
{
	const FVector MuzzleLocation = GetMuzzleLocation();
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

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(GetOwner());
	QueryParams.bTraceComplex = true;
	QueryParams.bReturnPhysicalMaterial = true;

	const float DynamicSpread = PelletSpreadAngle + (GetCurrentSpread() * 0.4f);
	const float HalfSpreadRad = FMath::DegreesToRadians(FMath::Max(0.0f, DynamicSpread * 0.5f));

	for (int32 PelletIndex = 0; PelletIndex < PelletCount; ++PelletIndex)
	{
		const FVector PelletAimDir = FMath::VRandCone(CameraAimDir, HalfSpreadRad);
		const FVector CameraTraceEnd = CameraLocation + PelletAimDir * MaxRange;

		FHitResult CameraHitResult;
		GetWorld()->LineTraceSingleByChannel(CameraHitResult, CameraLocation, CameraTraceEnd, ECC_Visibility, QueryParams);

		const FVector TargetLocation = CameraHitResult.bBlockingHit ? CameraHitResult.ImpactPoint : CameraTraceEnd;
		FHitResult WeaponHitResult;
		const bool bWeaponHit = GetWorld()->LineTraceSingleByChannel(WeaponHitResult, MuzzleLocation, TargetLocation, ECC_Visibility, QueryParams);

		const FVector DebugEndLocation = bWeaponHit ? WeaponHitResult.ImpactPoint : TargetLocation;
		DrawDebugLine(GetWorld(), MuzzleLocation, DebugEndLocation, FColor::Orange, false, 1.0f, 0, 0.6f);

		if (bWeaponHit)
		{
			const FVector FireDirection = (TargetLocation - MuzzleLocation).GetSafeNormal();
			ApplyShotgunDamage(WeaponHitResult, FireDirection);
			PlayImpactEffects(WeaponHitResult, FireDirection);
		}
	}

	PlayFireEffects(MuzzleLocation);
}

void ATimeThiefShotgun::ApplyRecoilAndSpread()
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

void ATimeThiefShotgun::ApplyShotgunDamage(const FHitResult& HitResult, const FVector& FireDirection)
{
	AActor* HitActor = HitResult.GetActor();
	if (!HitActor)
	{
		return;
	}

	AController* InstigatorController = nullptr;
	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		InstigatorController = OwnerPawn->GetController();
	}

	UGameplayStatics::ApplyPointDamage(
		HitActor,
		DamagePerPellet,
		FireDirection,
		HitResult,
		InstigatorController,
		this,
		nullptr
	);
}

void ATimeThiefShotgun::PlayFireEffects(const FVector& MuzzleLocation)
{
	if (MuzzleFlashEffect)
	{
		UGameplayStatics::SpawnEmitterAtLocation(this, MuzzleFlashEffect, MuzzleLocation, GetActorRotation());
	}

	if (FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, MuzzleLocation);
	}

	if (FireAnimation)
	{
		if (ATimeThiefCharacterBase* BaseChar = Cast<ATimeThiefCharacterBase>(GetOwner()))
		{
			BaseChar->PlayAnimationOnAllMeshes(FireAnimation, WeaponAnimSlot);
		}
	}
}

void ATimeThiefShotgun::PlayImpactEffects(const FHitResult& HitResult, const FVector& FireDirection)
{
	if (!ImpactEffect)
	{
		return;
	}

	const FVector IncomingDir = FireDirection.GetSafeNormal();
	const FVector ReflectDir = FMath::GetReflectionVector(IncomingDir, HitResult.ImpactNormal);
	const FRotator ImpactRotation = FRotationMatrix::MakeFromXZ(ReflectDir, HitResult.ImpactNormal).Rotator();

	UGameplayStatics::SpawnEmitterAtLocation(this, ImpactEffect, HitResult.ImpactPoint, ImpactRotation);
}


