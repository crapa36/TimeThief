#include "Weapon/Components/TimeThiefShotgunComponent.h"
#include "Animation/Player/TimeThiefPlayerAnimInstance.h"
#include "Character/TimeThiefCharacterBase.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "DrawDebugHelpers.h"
#include "Utils/Random32.h"

namespace 
{
	FVector MyRandomCone(const FVector& Direction, float ConeHalfAngleRad, float r1, float r2)
	{
		FVector helper = (FMath::Abs(Direction.X) > 0.1f) ? FVector(0, 1, 0) : FVector(1, 0, 0);
		FVector u = helper.Cross(Direction).GetSafeNormal();
		FVector v = Direction.Cross(u);
		
		const float cosMin = FMath::Cos(ConeHalfAngleRad);
		
		float cosTheta = 1.0f - r1 * (1.0f - cosMin);
		float sinTheta = FMath::Sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));
		float phi = 2.0f * PI * r2;
        
		FVector dir = {
			u * (FMath::Cos(phi) * sinTheta) +
			v * (FMath::Sin(phi) * sinTheta) +
			Direction * cosTheta
		};
        
		return dir.GetSafeNormal();
	}
	
}

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

TArray<FShotgunHitResult> UTimeThiefShotgunComponent::PerformPelletHitScan()
{
	TArray<FShotgunHitResult> Results;
	Results.Reserve(PelletCount);

	const FVector MuzzleLocation = GetMuzzleLocation();
	FVector CameraLocation = FVector::ZeroVector;
	FVector CameraAimDir = FVector::ForwardVector;
	ResolveFireAimView(CameraLocation, CameraAimDir);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetOwner());
	if (GetOwner())
	{
		QueryParams.AddIgnoredActor(GetOwner()->GetOwner());
	}
	QueryParams.bTraceComplex = true;
	QueryParams.bReturnPhysicalMaterial = true;

	const FVector CenterTraceEnd = CameraLocation + CameraAimDir * MaxRange;
	FHitResult CenterHitResult;
	GetWorld()->LineTraceSingleByChannel(CenterHitResult, CameraLocation, CenterTraceEnd, ECC_Visibility, QueryParams);
	const FVector CenterTargetLocation = CenterHitResult.bBlockingHit ? CenterHitResult.ImpactPoint : CenterTraceEnd;
	CacheLastShotSyncData(MuzzleLocation, (CenterTargetLocation - MuzzleLocation).GetSafeNormal());

	const float SpreadAngle = FMath::Max(0.0f, BaseSpread);
	const float HalfSpreadRad = FMath::DegreesToRadians(FMath::Max(0.0f, SpreadAngle * 0.5f));
	const uint32 RandomSeed = FMath::Rand();
	// TODO: 이 Seed 값을 패킷에 담아야 함!
	FRandom32 SeededRandom(RandomSeed);

	for (int32 PelletIndex = 0; PelletIndex < PelletCount; ++PelletIndex)
	{
		FShotgunHitResult PelletResult;

		// const FVector PelletAimDir = FMath::VRandCone(CameraAimDir, HalfSpreadRad);
		const float r1 = SeededRandom.NextFloat01();
		const float r2 = SeededRandom.NextFloat01();
		const FVector PelletAimDir = MyRandomCone(CameraAimDir, HalfSpreadRad, r1, r2);
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
	if (!GetOwner() || !GetOwner()->HasAuthority())
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

	for (const FShotgunHitResult& HitResult : HitResults)
	{
		if (!HitResult.bHit || !HitResult.HitActor.IsValid())
		{
			continue;
		}

		UGameplayStatics::ApplyPointDamage(
			HitResult.HitActor.Get(),
			DamagePerPellet + GetDamageBonus(),
			HitResult.FireDirection,
			HitResult.OriginalHitResult,
			InstigatorController,
			GetOwner(),
			nullptr
		);
	}

#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log, TEXT("[Shotgun][Damage] PelletBase=%.2f Bonus=%.2f FinalPerPellet=%.2f PelletCount=%d"), DamagePerPellet, GetDamageBonus(), DamagePerPellet + GetDamageBonus(), PelletCount);
#endif
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
			const float AppliedRecoilReduction = GetRecoilReduction();
			const float FinalVerticalRecoil = FMath::Max(0.0f, VerticalRecoil - AppliedRecoilReduction);
			const float FinalHorizontalRecoil = FMath::Max(0.0f, HorizontalRecoil - AppliedRecoilReduction);

#if !UE_BUILD_SHIPPING
			UE_LOG(LogTemp, Log, TEXT("[Shotgun][Recoil] Reduction=%.3f FinalV=%.3f FinalH=%.3f"), AppliedRecoilReduction, FinalVerticalRecoil, FinalHorizontalRecoil);
#endif

			const FVector2D RecoilDelta = AnimInst->ApplyFireSpread(FinalVerticalRecoil, FinalHorizontalRecoil, 0.0f, 0.0f);

			if (APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController()))
			{
				PC->AddPitchInput(-RecoilDelta.Y);
				PC->AddYawInput(RecoilDelta.X);
			}
		}
	}
}