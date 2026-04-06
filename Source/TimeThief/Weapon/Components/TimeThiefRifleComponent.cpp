#include "Weapon/Components/TimeThiefRifleComponent.h"
#include "Weapon/TimeThiefMasterWeapon.h"
#include "Animation/Player/TimeThiefPlayerAnimInstance.h"
#include "Character/TimeThiefCharacterBase.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

UTimeThiefRifleComponent::UTimeThiefRifleComponent()
{
	RoundsPerSecond = 10.0f;
}

void UTimeThiefRifleComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsFiring() && CurrentSpread > BaseSpread)
	{
		CurrentSpread = FMath::FInterpConstantTo(CurrentSpread, BaseSpread, DeltaTime, SpreadDecreasePerSecond);
	}
}

void UTimeThiefRifleComponent::ExecuteFireShot()
{
	const FRifleHitResult HitResult = PerformHitScan();
	ApplyDamage(HitResult);
	PlayFireEffects();
	PlayImpactEffects(HitResult);
}

FRifleHitResult UTimeThiefRifleComponent::PerformHitScan() const
{
	FRifleHitResult Result;

	FVector CameraLocation = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
	FVector CameraAimDir = GetOwner() ? GetOwner()->GetActorForwardVector() : FVector::ForwardVector;

	if (AActor* MasterWeapon = GetOwner())
	{
		if (APawn* OwnerPawn = Cast<APawn>(MasterWeapon->GetOwner()))
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
	}

	const float SpreadAngle = GetSpreadAngleForFire();
	if (SpreadAngle > 0.0f)
	{
		const float HalfSpread = FMath::DegreesToRadians(SpreadAngle * 0.5f);
		CameraAimDir = FMath::VRandCone(CameraAimDir, HalfSpread);
	}

	const FVector TraceEnd = CameraLocation + CameraAimDir * MaxRange;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetOwner());
	if (GetOwner())
	{
		QueryParams.AddIgnoredActor(GetOwner()->GetOwner());
	}
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

void UTimeThiefRifleComponent::ApplyDamage(const FRifleHitResult& HitResult)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (!HitResult.HitActor.IsValid())
	{
		return;
	}

	AController* InstigatorController = nullptr;
	if (AActor* MasterWeapon = GetOwner())
	{
		if (APawn* OwnerPawn = Cast<APawn>(MasterWeapon->GetOwner()))
		{
			InstigatorController = OwnerPawn->GetController();
		}
	}

	UGameplayStatics::ApplyPointDamage(
		HitResult.HitActor.Get(),
		BaseDamage,
		HitResult.FireDirection,
		HitResult.OriginalHitResult,
		InstigatorController,
		GetOwner(),
		nullptr
	);
}

void UTimeThiefRifleComponent::PlayFireEffects()
{
	const FVector MuzzleLoc = GetMuzzleLocation();

	if (MuzzleFlashEffect)
	{
		UGameplayStatics::SpawnEmitterAtLocation(
			this,
			MuzzleFlashEffect,
			MuzzleLoc,
			GetOwner() ? GetOwner()->GetActorRotation() : FRotator::ZeroRotator
		);
	}

	if (FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, MuzzleLoc);
	}

	if (FireAnimation)
	{
		if (AActor* MasterWeapon = GetOwner())
		{
			if (ATimeThiefCharacterBase* BaseChar = Cast<ATimeThiefCharacterBase>(MasterWeapon->GetOwner()))
			{
				BaseChar->PlayAnimationOnAllMeshes(FireAnimation, WeaponAnimSlot);
			}
		}
	}
}

void UTimeThiefRifleComponent::PlayImpactEffects(const FRifleHitResult& HitResult)
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

void UTimeThiefRifleComponent::ApplyRecoilAndSpread()
{
	if (!GetOwner())
	{
		return;
	}

	APawn* OwnerPawn = Cast<APawn>(GetOwner()->GetOwner());
	if (!OwnerPawn)
	{
		return;
	}

	if (ACharacter* OwnerChar = Cast<ACharacter>(OwnerPawn))
	{
		if (UTimeThiefPlayerAnimInstance* AnimInst = Cast<UTimeThiefPlayerAnimInstance>(OwnerChar->GetMesh()->GetAnimInstance()))
		{
			AnimInst->SetRecoilRecoverySpeed(RecoilRecoverySpeed, SpreadDecreasePerSecond);
			CurrentSpread = FMath::Clamp(CurrentSpread + SpreadIncreasePerShot, BaseSpread, MaxSpread);
			const FVector2D RecoilDelta = AnimInst->ApplyFireSpread(MaxVerticalRecoil, MaxHorizontalRecoil, RecoilBuildupPerShot, SpreadIncreasePerShot);

			if (APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController()))
			{
				PC->AddPitchInput(-RecoilDelta.Y);
				PC->AddYawInput(RecoilDelta.X);
			}
		}
	}
}