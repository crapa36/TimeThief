#include "Weapon/Components/TimeThiefRifleComponent.h"
#include "Weapon/TimeThiefMasterWeapon.h"
#include "Animation/Player/TimeThiefPlayerAnimInstance.h"
#include "Character/TimeThiefCharacterBase.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Utils/TimeThiefAimStatics.h"

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

FRifleHitResult UTimeThiefRifleComponent::PerformHitScan()
{
	FRifleHitResult Result;

	FVector CameraLocation = FVector::ZeroVector;
	FVector CameraAimDir = FVector::ForwardVector;
	ResolveFireAimView(CameraLocation, CameraAimDir);

	const float SpreadAngle = GetSpreadAngleForFire();
	if (SpreadAngle > 0.0f)
	{
		const float HalfSpread = FMath::DegreesToRadians(SpreadAngle * 0.5f);
		CameraAimDir = FMath::VRandCone(CameraAimDir, HalfSpread);
	}

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Reserve(2);
	ActorsToIgnore.Add(GetOwner());
	if (GetOwner())
	{
		ActorsToIgnore.Add(GetOwner()->GetParentActor());
	}

	FHitResult CameraHitResult;
	FVector TraceEnd;
	UTimeThiefAimStatics::TraceFromView(
		GetWorld(),
		CameraLocation,
		CameraAimDir,
		MaxRange,
		ActorsToIgnore,
		CameraHitResult,
		TraceEnd,
		ECC_Visibility,
		true,
		true);

	const FVector TargetLocation = CameraHitResult.bBlockingHit ? CameraHitResult.ImpactPoint : TraceEnd;
	const FVector MuzzleLocation = GetMuzzleLocation();

	FHitResult WeaponHitResult;
	const bool bWeaponHit = UTimeThiefAimStatics::TraceLine(
		GetWorld(),
		MuzzleLocation,
		TargetLocation,
		ActorsToIgnore,
		WeaponHitResult,
		ECC_Visibility,
		true,
		true);

	const FVector DebugEndLocation = bWeaponHit ? WeaponHitResult.ImpactPoint : TargetLocation;
	DrawDebugLine(GetWorld(), MuzzleLocation, DebugEndLocation, FColor::Red, false, 2.0f, 0, 1.0f);
	if (bWeaponHit)
	{
		DrawDebugPoint(GetWorld(), DebugEndLocation, 5.0f, FColor::Green, false, 2.0f);
	}

	Result.FireDirection = UTimeThiefAimStatics::ResolveAimDirectionToTarget(MuzzleLocation, TargetLocation, CameraAimDir);
	CacheLastShotSyncData(MuzzleLocation, Result.FireDirection);

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
		BaseDamage + GetDamageBonus(),
		HitResult.FireDirection,
		HitResult.OriginalHitResult,
		InstigatorController,
		GetOwner(),
		nullptr
	);

#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log, TEXT("[Rifle][Damage] Base=%.2f Bonus=%.2f Final=%.2f"), BaseDamage, GetDamageBonus(), BaseDamage + GetDamageBonus());
#endif
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
	
	if (ATimeThiefCharacterBase* OwnerChar = Cast<ATimeThiefCharacterBase>(GetOwner()->GetParentActor()))
	{
		if (UTimeThiefPlayerAnimInstance* AnimInst = Cast<UTimeThiefPlayerAnimInstance>(OwnerChar->GetThirdPersonMesh()->GetAnimInstance()))
		{
			AnimInst->SetRecoilRecoverySpeed(RecoilRecoverySpeed, SpreadDecreasePerSecond);
			CurrentSpread = FMath::Clamp(CurrentSpread + SpreadIncreasePerShot, BaseSpread, MaxSpread);
			const float AppliedRecoilReduction = GetRecoilReduction();
			const float FinalVerticalRecoil = FMath::Max(0.0f, MaxVerticalRecoil - AppliedRecoilReduction);
			const float FinalHorizontalRecoil = FMath::Max(0.0f, MaxHorizontalRecoil - AppliedRecoilReduction);
			const float FinalRecoilBuildup = FMath::Max(0.0f, RecoilBuildupPerShot - AppliedRecoilReduction);

#if !UE_BUILD_SHIPPING
			UE_LOG(LogTemp, Log, TEXT("[Rifle][Recoil] Reduction=%.3f FinalV=%.3f FinalH=%.3f FinalBuildup=%.3f"), AppliedRecoilReduction, FinalVerticalRecoil, FinalHorizontalRecoil, FinalRecoilBuildup);
#endif

			const FVector2D RecoilDelta = AnimInst->ApplyFireSpread(
				FinalVerticalRecoil,
				FinalHorizontalRecoil,
				FinalRecoilBuildup,
				SpreadIncreasePerShot
			);

			if (APlayerController* PC = Cast<APlayerController>(OwnerChar->GetController()))
			{
				PC->AddPitchInput(-RecoilDelta.Y);
				PC->AddYawInput(RecoilDelta.X);
			}
		}
	}
}

void UTimeThiefRifleComponent::SetWeaponStatForNetwork(const FWeaponStatData& InStatData)
{
	Super::SetWeaponStatForNetwork(InStatData);
	
	// NONE (Rifle은 기본 Stat 외의 추가 stat이 없다)
}
