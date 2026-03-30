#include "Weapon/Components/TimeThiefShotgunComponent.h"
#include "Weapon/TimeThiefMasterWeapon.h"
#include "Animation/Player/TimeThiefPlayerAnimInstance.h"
#include "Character/TimeThiefCharacterBase.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "DrawDebugHelpers.h"

UTimeThiefShotgunComponent::UTimeThiefShotgunComponent()
{
	FireRate = 110.0f;
	RoundsPerSecond = 110.0f / 60.0f;
	MaxAmmo = 8;
	ReloadTime = 2.0f;
	MaxSpread = 7.0f;
	SpreadIncreasePerShot = 0.f;
	SpreadDecreasePerSecond = 0.f;
	BaseSpread = 3.5f;
}

void UTimeThiefShotgunComponent::BeginPlay()
{
	Super::BeginPlay();
	CurrentSpread = BaseSpread;
}

void UTimeThiefShotgunComponent::ExecuteFireShot()
{
	const TArray<FShotgunHitResult> HitResults = PerformPelletHitScan();
	ApplyDamage(HitResults);
	PlayFireEffects();
	PlayImpactEffects(HitResults);
}

TArray<FShotgunHitResult> UTimeThiefShotgunComponent::PerformPelletHitScan() const
{
	TArray<FShotgunHitResult> Results;
	Results.Reserve(PelletCount);

	const FVector MuzzleLocation = GetMuzzleLocation();
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

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetOwner());
	if (GetOwner())
	{
		QueryParams.AddIgnoredActor(GetOwner()->GetOwner());
	}
	QueryParams.bTraceComplex = true;
	QueryParams.bReturnPhysicalMaterial = true;

	const float SpreadAngle = FMath::Max(0.0f, BaseSpread);
	const float HalfSpreadRad = FMath::DegreesToRadians(FMath::Max(0.0f, SpreadAngle * 0.5f));

	for (int32 PelletIndex = 0; PelletIndex < PelletCount; ++PelletIndex)
	{
		FShotgunHitResult PelletResult;

		const FVector PelletAimDir = FMath::VRandCone(CameraAimDir, HalfSpreadRad);
		const FVector CameraTraceEnd = CameraLocation + PelletAimDir * MaxRange;

		FHitResult CameraHitResult;
		GetWorld()->LineTraceSingleByChannel(CameraHitResult, CameraLocation, CameraTraceEnd, ECC_Visibility, QueryParams);

		const FVector TargetLocation = CameraHitResult.bBlockingHit ? CameraHitResult.ImpactPoint : CameraTraceEnd;
		FHitResult WeaponHitResult;
		const bool bWeaponHit = GetWorld()->LineTraceSingleByChannel(WeaponHitResult, MuzzleLocation, TargetLocation, ECC_Visibility, QueryParams);

		const FVector DebugEndLocation = bWeaponHit ? WeaponHitResult.ImpactPoint : TargetLocation;
		DrawDebugLine(GetWorld(), MuzzleLocation, DebugEndLocation, FColor::Orange, false, 2.0f, 0, 1.0f);
		if (bWeaponHit)
		{
			DrawDebugPoint(GetWorld(), DebugEndLocation, 5.0f, FColor::Green, false, 2.0f);
		}

		PelletResult.FireDirection = (TargetLocation - MuzzleLocation).GetSafeNormal();
		if (bWeaponHit)
		{
			PelletResult.bHit = true;
			PelletResult.HitLocation = WeaponHitResult.ImpactPoint;
			PelletResult.HitNormal = WeaponHitResult.ImpactNormal;
			PelletResult.HitActor = WeaponHitResult.GetActor();
			PelletResult.OriginalHitResult = WeaponHitResult;
		}

		Results.Add(PelletResult);
	}

	return Results;
}

void UTimeThiefShotgunComponent::ApplyDamage(const TArray<FShotgunHitResult>& HitResults)
{
	AController* InstigatorController = nullptr;
	if (AActor* MasterWeapon = GetOwner())
	{
		if (APawn* OwnerPawn = Cast<APawn>(MasterWeapon->GetOwner()))
		{
			InstigatorController = OwnerPawn->GetController();
		}
	}

	for (const FShotgunHitResult& HitResult : HitResults)
	{
		if (!HitResult.bHit || !HitResult.HitActor.IsValid())
		{
			continue;
		}

		UGameplayStatics::ApplyPointDamage(
			HitResult.HitActor.Get(),
			DamagePerPellet,
			HitResult.FireDirection,
			HitResult.OriginalHitResult,
			InstigatorController,
			GetOwner(),
			nullptr
		);
	}
}

void UTimeThiefShotgunComponent::PlayFireEffects()
{
	const FVector MuzzleLocation = GetMuzzleLocation();

	if (MuzzleFlashEffect)
	{
		UGameplayStatics::SpawnEmitterAtLocation(this, MuzzleFlashEffect, MuzzleLocation, GetOwner() ? GetOwner()->GetActorRotation() : FRotator::ZeroRotator);
	}

	if (FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, MuzzleLocation);
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

void UTimeThiefShotgunComponent::PlayImpactEffects(const TArray<FShotgunHitResult>& HitResults)
{
	if (!ImpactEffect)
	{
		return;
	}

	for (const FShotgunHitResult& HitResult : HitResults)
	{
		if (!HitResult.bHit)
		{
			continue;
		}

		const FVector IncomingDir = HitResult.FireDirection.GetSafeNormal();
		const FVector ReflectDir = FMath::GetReflectionVector(IncomingDir, HitResult.HitNormal);
		const FRotator ImpactRotation = FRotationMatrix::MakeFromXZ(ReflectDir, HitResult.HitNormal).Rotator();

		UGameplayStatics::SpawnEmitterAtLocation(this, ImpactEffect, HitResult.HitLocation, ImpactRotation);
	}
}

void UTimeThiefShotgunComponent::ApplyRecoilAndSpread()
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
			AnimInst->SetRecoilRecoverySpeed(0.f, SpreadDecreasePerSecond);
			const FVector2D RecoilDelta = AnimInst->ApplyFireSpread(VerticalRecoil, HorizontalRecoil, 0.0f, 0.0f);

			if (APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController()))
			{
				PC->AddPitchInput(-RecoilDelta.Y);
				PC->AddYawInput(RecoilDelta.X);
			}
		}
	}
}