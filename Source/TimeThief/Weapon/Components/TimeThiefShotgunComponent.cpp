#include "Weapon/Components/TimeThiefShotgunComponent.h"
#include "Animation/Player/TimeThiefPlayerAnimInstance.h"
#include "Character/TimeThiefCharacterBase.h"
#include "CollisionQueryParams.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Smoke/TimeThiefSmokeWorldSubsystem.h"
#include "Weapon/TimeThiefMasterWeapon.h"
#include "Weapon/TimeThiefWeaponTrail.h"
#include "Utils/Random32.h"
#include "Utils/TimeThiefAimStatics.h"

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
	MaxSpread = 14.0f;
	SpreadIncreasePerShot = 0.f;
	SpreadDecreasePerSecond = 0.f;
	BaseSpread = 7.0f;
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

uint32 UTimeThiefShotgunComponent::GetCombatAttackShotSeed() const
{
	return LastShotSeed;
}

void UTimeThiefShotgunComponent::SetRemoteShotSeed(uint32 InShotSeed)
{
	RemoteShotSeed = InShotSeed;
	bHasRemoteShotSeed = true;
}

TArray<FShotgunHitResult> UTimeThiefShotgunComponent::PerformPelletHitScan()
{
	TArray<FShotgunHitResult> Results;
	Results.Reserve(PelletCount);

	UWorld* World = GetWorld();
	if (!World)
	{
		return Results;
	}

	AActor* OwnerActor = GetOwner();
	const FVector MuzzleLocation = GetMuzzleLocation();
	FVector CameraLocation = FVector::ZeroVector;
	FVector CameraAimDir = FVector::ForwardVector;
	ResolveFireAimView(CameraLocation, CameraAimDir);
	CameraAimDir = UTimeThiefAimStatics::NormalizeAimDirection(CameraAimDir);

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Reserve(2);
	ActorsToIgnore.Add(OwnerActor);
	if (OwnerActor)
	{
		ActorsToIgnore.Add(OwnerActor->GetParentActor());
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TimeThiefShotgunTrace), true);
	QueryParams.bReturnPhysicalMaterial = true;
	QueryParams.AddIgnoredActors(ActorsToIgnore);

	FVector CenterTraceEnd = UTimeThiefAimStatics::ResolveAimTargetLocation(CameraLocation, CameraAimDir, MaxRange);
	FHitResult CenterHitResult;
	UTimeThiefAimStatics::TraceFromViewWithParams(
		World,
		CameraLocation,
		CameraAimDir,
		MaxRange,
		QueryParams,
		CenterHitResult,
		CenterTraceEnd,
		ECC_Visibility);
	const FVector CenterTargetLocation = CenterHitResult.bBlockingHit ? CenterHitResult.ImpactPoint : CenterTraceEnd;
	CacheLastShotSyncData(
		MuzzleLocation,
		UTimeThiefAimStatics::ResolveAimDirectionToTarget(MuzzleLocation, CenterTargetLocation, CameraAimDir));

	ATimeThiefMasterWeapon* MasterWeapon = Cast<ATimeThiefMasterWeapon>(OwnerActor);
	if (!MasterWeapon)
	{
		return Results;
	}
	UTimeThiefWeaponTrail* WeaponTrail = MasterWeapon->GetWeaponTrail();
	if (!WeaponTrail)
	{
		return Results;
	}
	UTimeThiefSmokeWorldSubsystem* SmokeSubsystem = World->GetSubsystem<UTimeThiefSmokeWorldSubsystem>();

	const float SpreadAngle = FMath::Max(0.0f, BaseSpread);
	const float HalfSpreadRad = FMath::DegreesToRadians(FMath::Max(0.0f, SpreadAngle * 0.5f));
	const uint32 RandomSeed = bHasRemoteShotSeed ? RemoteShotSeed : FMath::Rand();
	bHasRemoteShotSeed = false;
	LastShotSeed = RandomSeed;
	FRandom32 SeededRandom(RandomSeed);

	for (int32 PelletIndex = 0; PelletIndex < PelletCount; ++PelletIndex)
	{
		FShotgunHitResult PelletResult;

		// const FVector PelletAimDir = FMath::VRandCone(CameraAimDir, HalfSpreadRad);
		const float r1 = SeededRandom.NextFloat01();
		const float r2 = SeededRandom.NextFloat01();
		const FVector PelletAimDir = MyRandomCone(CameraAimDir, HalfSpreadRad, r1, r2);
		FVector CameraTraceEnd = UTimeThiefAimStatics::ResolveAimTargetLocation(CameraLocation, PelletAimDir, MaxRange);

		FHitResult CameraHitResult;
		UTimeThiefAimStatics::TraceFromViewWithParams(
			World,
			CameraLocation,
			PelletAimDir,
			MaxRange,
			QueryParams,
			CameraHitResult,
			CameraTraceEnd,
			ECC_Visibility);

		const FVector TargetLocation = CameraHitResult.bBlockingHit ? CameraHitResult.ImpactPoint : CameraTraceEnd;
		FHitResult WeaponHitResult;
		const bool bWeaponHit = UTimeThiefAimStatics::TraceLineWithParams(
			World,
			MuzzleLocation,
			TargetLocation,
			QueryParams,
			WeaponHitResult,
			ECC_Visibility);

		const FVector TrailEndLocation = bWeaponHit ? WeaponHitResult.ImpactPoint : TargetLocation;
		WeaponTrail->DrawHitscanTrail(
			*World,
			ETimeThiefWeaponTrailType::ShotgunPellet,
			MuzzleLocation,
			TrailEndLocation);

		if (SmokeSubsystem)
		{
			SmokeSubsystem->SubmitBulletTrace(MuzzleLocation, TrailEndLocation, 0.65f, static_cast<int32>(RandomSeed + PelletIndex * 104729u));
		}

		PelletResult.FireDirection = UTimeThiefAimStatics::ResolveAimDirectionToTarget(MuzzleLocation, TargetLocation, PelletAimDir);
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
			GetEffectiveDamage(DamagePerPellet),
			HitResult.FireDirection,
			HitResult.OriginalHitResult,
			InstigatorController,
			GetOwner(),
			nullptr
		);
	}

#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log, TEXT("[Shotgun][Damage] PelletBase=%.2f Bonus=%.2f Multiplier=%.2f FinalPerPellet=%.2f PelletCount=%d"), DamagePerPellet, GetDamageBonus(), GetDamageMultiplier(), GetEffectiveDamage(DamagePerPellet), PelletCount);
#endif
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
	
	if (ACharacter* OwnerChar = Cast<ACharacter>(GetOwner()->GetParentActor()))
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

			if (APlayerController* PC = Cast<APlayerController>(OwnerChar->GetController()))
			{
				PC->AddPitchInput(-RecoilDelta.Y);
				PC->AddYawInput(RecoilDelta.X);
			}
		}
	}
}

FWeaponStatData UTimeThiefShotgunComponent::GetWeaponStatDataForNetwork() const
{
	FWeaponStatData StatData = Super::GetWeaponStatDataForNetwork();
	StatData.PelletCount = PelletCount;
	StatData.ConeAngle = BaseSpread;
	
	return StatData;
}

void UTimeThiefShotgunComponent::SetWeaponStatForNetwork(const FWeaponStatData& InStatData)
{
	Super::SetWeaponStatForNetwork(InStatData);
	
	PelletCount = InStatData.PelletCount;
	BaseSpread = InStatData.ConeAngle;		// Spread가 Shotgun에선 ConeAngle로 사용됨 (이게 괜찮은 구조인가?)
	
	// Test용 로그
	// UE_LOG(LogTemp, Log, TEXT("[Shotgun][NetworkStat] PelletCount=%d ConeAngle=%.2f"), PelletCount, BaseSpread);
}
