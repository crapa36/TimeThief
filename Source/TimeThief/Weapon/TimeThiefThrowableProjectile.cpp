#include "Weapon/TimeThiefThrowableProjectile.h"

#include "Actors/TimeThiefSmokeVolume.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "TimeThiefSmokeParameterDefaults.h"
#include "Smoke/TimeThiefSmokeWorldSubsystem.h"
#include "TimerManager.h"
#include "Weapon/TimeThiefWeaponTrail.h"
#include "Components/ThrowableNetworkSyncComponent.h"

ATimeThiefThrowableProjectile::ATimeThiefThrowableProjectile()
{
	PrimaryActorTick.bCanEverTick = false;

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	SetRootComponent(CollisionComponent);
	CollisionComponent->InitSphereRadius(ActiveSettings.CollisionRadius);
	CollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Block);

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(CollisionComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetGenerateOverlapEvents(false);
	MeshComponent->SetHiddenInGame(false);
	MeshComponent->SetVisibility(true);

	ProjectileMovementComponent = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovementComponent"));
	ProjectileMovementComponent->ProjectileGravityScale = ActiveSettings.GravityScale;
	ProjectileMovementComponent->bRotationFollowsVelocity = true;
	ProjectileMovementComponent->bShouldBounce = ActiveSettings.bShouldBounce;
	ProjectileMovementComponent->Bounciness = ActiveSettings.Bounciness;
	ProjectileMovementComponent->Friction = ActiveSettings.Friction;
	ProjectileMovementComponent->bAutoActivate = false;
	ProjectileMovementComponent->OnProjectileBounce.AddDynamic(this, &ATimeThiefThrowableProjectile::OnProjectileBounce);

	WeaponTrail = CreateDefaultSubobject<UTimeThiefWeaponTrail>(TEXT("WeaponTrail"));
	
	ThrowableNetworkSyncComponent = CreateDefaultSubobject<UThrowableNetworkSyncComponent>(TEXT("ThrowableNetworkSyncComponent"));
}

void ATimeThiefThrowableProjectile::InitializeThrowable(EItemID InItemID, AActor* InOwnerActor, APawn* InInstigatorPawn)
{
	InitializeThrowable(InItemID, InOwnerActor, InInstigatorPawn, FTimeThiefThrowableProjectileSettings());
}

void ATimeThiefThrowableProjectile::InitializeThrowable(EItemID InItemID, AActor* InOwnerActor, APawn* InInstigatorPawn, const FTimeThiefThrowableProjectileSettings& InProjectileSettings)
{
	ThrowableItemID = InItemID;
	ActiveSettings = InProjectileSettings;
	ApplyProjectileSettings();
	CachedOwnerActor = InOwnerActor;
	CachedInstigatorPawn = InInstigatorPawn;
	SetOwner(InOwnerActor);
	SetInstigator(InInstigatorPawn);

	if (CollisionComponent)
	{
		CollisionComponent->ClearMoveIgnoreActors();

		if (InOwnerActor)
		{
			CollisionComponent->IgnoreActorWhenMoving(InOwnerActor, true);
		}
		if (InInstigatorPawn && InInstigatorPawn != InOwnerActor)
		{
			CollisionComponent->IgnoreActorWhenMoving(InInstigatorPawn, true);
		}
	}
}

void ATimeThiefThrowableProjectile::LaunchThrowable(const FVector& InitialVelocity, float InFuseTime)
{
	if (InitialVelocity.IsNearlyZero())
	{
		Destroy();
		return;
	}

	bExploded = false;
	SetActorHiddenInGame(false);
	SetActorRotation(InitialVelocity.Rotation());

	if (CollisionComponent)
	{
		CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	if (ProjectileMovementComponent)
	{
		ProjectileMovementComponent->ProjectileGravityScale = ActiveSettings.GravityScale;
		ProjectileMovementComponent->bShouldBounce = ActiveSettings.bShouldBounce;
		ProjectileMovementComponent->Bounciness = ActiveSettings.Bounciness;
		ProjectileMovementComponent->Friction = ActiveSettings.Friction;
		ProjectileMovementComponent->SetUpdatedComponent(CollisionComponent);
		ProjectileMovementComponent->Velocity = InitialVelocity;
		ProjectileMovementComponent->Activate(true);
	}

	StartGrenadeTrail();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(FuseTimerHandle, this, &ATimeThiefThrowableProjectile::HandleFuseExpired, FMath::Max(0.1f, InFuseTime), false);
	}
}

void ATimeThiefThrowableProjectile::SetThrowableMesh()
{
	if (MeshComponent)
	{
		UStaticMesh* MeshToUse = ActiveSettings.MeshOverride.Get();
		MeshComponent->SetStaticMesh(MeshToUse);
		MeshComponent->SetRelativeLocation(FVector::ZeroVector);
		MeshComponent->SetRelativeRotation(FRotator::ZeroRotator);
		MeshComponent->SetHiddenInGame(false);
		MeshComponent->SetVisibility(true, true);

		if (MeshToUse)
		{
			const float MeshRadius = MeshToUse->GetBounds().SphereRadius;
			const float Scale = MeshRadius > KINDA_SMALL_NUMBER ? ActiveSettings.MeshVisualRadius / MeshRadius : 1.0f;
			MeshComponent->SetRelativeScale3D(FVector(Scale));
#if !UE_BUILD_SHIPPING
			UE_LOG(LogTemp, Warning, TEXT("[ThrowableDebug][Projectile] Mesh assigned: %s BoundsRadius=%.3f VisualScale=%.3f"),
				*GetNameSafe(MeshToUse),
				MeshRadius,
				Scale);
#endif
		}
		else
		{
			MeshComponent->SetRelativeScale3D(FVector::OneVector);
#if !UE_BUILD_SHIPPING
			UE_LOG(LogTemp, Warning, TEXT("[ThrowableDebug][Projectile] Mesh assignment failed: MeshOverride is not assigned for ItemID=%d."),
				static_cast<int32>(ThrowableItemID));
#endif
		}
	}
}

void ATimeThiefThrowableProjectile::InitializeNetworkSyncAsLocalOwner(uint32 ObjectId)
{
	if (ThrowableNetworkSyncComponent)
	{
		ThrowableNetworkSyncComponent->InitializeAsLocalOwner(ObjectId);
	}
}

void ATimeThiefThrowableProjectile::InitializeNetworkSyncAsRemoteProxy(uint32 ObjectId)
{
	if (ThrowableNetworkSyncComponent)
	{
		ThrowableNetworkSyncComponent->InitializeAsRemoteProxy(ObjectId);
	}

	StartGrenadeTrail();
}

void ATimeThiefThrowableProjectile::PushRemoteMoveSnapshot(const FThrowableMoveSnapshot& Snapshot)
{
	if (ThrowableNetworkSyncComponent)
	{
		ThrowableNetworkSyncComponent->PushRemoteSnapshot(Snapshot);
	}
}

void ATimeThiefThrowableProjectile::RemoteExplosionEffect(const FVector& ExplosionLocation)
{
	UE_LOG(LogTemp, Log, TEXT("[Throwable] Remote explosion effect triggered. Location=%s"), *ExplosionLocation.ToString()); 
	
	// 서버 기준 폭발 위치로 보정
	SetActorLocation(ExplosionLocation);

	// 중복 방지
	if (bExploded)
	{
		return;
	}

	bExploded = true;

	// Trail 제거
	if (WeaponTrail)
	{
		WeaponTrail->StopProjectileTrail(ActiveTrailComponent);
		ActiveTrailComponent = nullptr;
	}

	// 이동 중단
	if (ProjectileMovementComponent)
	{
		ProjectileMovementComponent->StopMovementImmediately();
		ProjectileMovementComponent->Deactivate();
	}

	// 충돌 제거
	if (CollisionComponent)
	{
		CollisionComponent->SetCollisionEnabled(
			ECollisionEnabled::NoCollision);
	}

	// FX / Sound만 재생
	PlayDetonationEffects(ExplosionLocation);

	if (ThrowableItemID == EItemID::SmokeGrenade)
	{
		SpawnSmokeVolume(ExplosionLocation);
	}

	Destroy();
}

void ATimeThiefThrowableProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	WeaponTrail->StopProjectileTrail(ActiveTrailComponent);
	ActiveTrailComponent = nullptr;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FuseTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void ATimeThiefThrowableProjectile::HandleFuseExpired()
{
	ExplodeOnce();
}

void ATimeThiefThrowableProjectile::ExplodeOnce()
{
	if (bExploded)
	{
		return;
	}
	
	if (ThrowableNetworkSyncComponent)
	{
		ThrowableNetworkSyncComponent->SendExplosionSync();
	}

	bExploded = true;
	const FVector EffectLocation = GetActorLocation();
	WeaponTrail->StopProjectileTrail(ActiveTrailComponent);
	ActiveTrailComponent = nullptr;

	if (ProjectileMovementComponent)
	{
		ProjectileMovementComponent->StopMovementImmediately();
		ProjectileMovementComponent->Deactivate();
	}

	if (CollisionComponent)
	{
		CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	switch (ThrowableItemID)
	{
	case EItemID::Grenade:
	case EItemID::SmokeGrenade:
		break;
	default:
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Warning, TEXT("[Throwable] Unsupported throwable item exploded. ItemID=%d"), static_cast<int32>(ThrowableItemID));
#endif
		break;
	}

	if (ActiveSettings.bApplyRadialDamage)
	{
		ApplyRadialThrowableDamage(EffectLocation);
	}

	PlayDetonationEffects(EffectLocation);

	if (ActiveSettings.bApplyRadialDamage && ActiveSettings.bDrawDamageDebug)
	{
		if (UWorld* World = GetWorld())
		{
			DrawDebugSphere(World, EffectLocation, ActiveSettings.DamageOuterRadius, FMath::Max(4, ActiveSettings.DebugSegments), FColor::Red, false, ActiveSettings.DamageDebugDuration, 0, 1.5f);
			DrawDebugSphere(World, EffectLocation, ActiveSettings.DamageInnerRadius, FMath::Max(4, ActiveSettings.DebugSegments), FColor::Yellow, false, ActiveSettings.DamageDebugDuration, 0, 1.0f);
		}
	}

	if (ThrowableItemID == EItemID::SmokeGrenade)
	{
		SpawnSmokeVolume(EffectLocation);
	}
	else if (ThrowableItemID == EItemID::Grenade)
	{
		if (UTimeThiefSmokeWorldSubsystem* SmokeSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UTimeThiefSmokeWorldSubsystem>() : nullptr)
		{
			SmokeSubsystem->SubmitExplosion(
				EffectLocation,
				FMath::Max(TimeThiefSmokeParameterDefaults::ExplosionShockRadius, ActiveSettings.DamageOuterRadius),
				1.0f,
				FMath::Rand());
		}
	}

	Destroy();
}

void ATimeThiefThrowableProjectile::ApplyRadialThrowableDamage(const FVector& ExplosionLocation)
{
	if (!HasAuthority())
	{
		return;
	}

	AController* InstigatorController = nullptr;
	if (APawn* InstigatorPawn = CachedInstigatorPawn.Get())
	{
		InstigatorController = InstigatorPawn->GetController();
	}

	TArray<AActor*> IgnoreActors;
	IgnoreActors.Add(this);

	UGameplayStatics::ApplyRadialDamageWithFalloff(
		this,
		ActiveSettings.MaxDamage,
		ActiveSettings.MinDamage,
		ExplosionLocation,
		ActiveSettings.DamageInnerRadius,
		ActiveSettings.DamageOuterRadius,
		1.0f,
		nullptr,
		IgnoreActors,
		this,
		InstigatorController,
		ECC_Visibility);
}

void ATimeThiefThrowableProjectile::PlayDetonationEffects(const FVector& ExplosionLocation)
{
	UNiagaraSystem* DetonationEffect = ActiveSettings.DetonationNiagaraEffect.Get();
	if (DetonationEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, DetonationEffect, ExplosionLocation);
	}

	if (ActiveSettings.ExplosionSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ActiveSettings.ExplosionSound, ExplosionLocation);
	}
}

void ATimeThiefThrowableProjectile::SpawnSmokeVolume(const FVector& SmokeLocation)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = CachedOwnerActor.Get();
	SpawnParams.Instigator = CachedInstigatorPawn.Get();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ATimeThiefSmokeVolume* SmokeVolume = World->SpawnActor<ATimeThiefSmokeVolume>(
		ATimeThiefSmokeVolume::StaticClass(),
		SmokeLocation,
		FRotator::ZeroRotator,
		SpawnParams);

	if (SmokeVolume)
	{
		SmokeVolume->InitializeSmokeVolume(CachedOwnerActor.Get(), CachedInstigatorPawn.Get());
	}
}

void ATimeThiefThrowableProjectile::PlayCollisionSound(const FHitResult& ImpactResult, const FVector& ImpactVelocity)
{
	if (bExploded || !ActiveSettings.CollisionSound)
	{
		return;
	}

	if (ImpactVelocity.Size() < ActiveSettings.CollisionSoundMinSpeed)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float CurrentTime = World->GetTimeSeconds();
	if (CurrentTime - LastCollisionSoundTime < ActiveSettings.CollisionSoundCooldown)
	{
		return;
	}

	LastCollisionSoundTime = CurrentTime;
	const FVector SoundLocation = ImpactResult.bBlockingHit ? FVector(ImpactResult.ImpactPoint) : GetActorLocation();
	UGameplayStatics::PlaySoundAtLocation(this, ActiveSettings.CollisionSound, SoundLocation);
}

void ATimeThiefThrowableProjectile::OnProjectileBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity)
{
	PlayCollisionSound(ImpactResult, ImpactVelocity);
}

void ATimeThiefThrowableProjectile::ApplyProjectileSettings()
{
	if (CollisionComponent)
	{
		CollisionComponent->InitSphereRadius(ActiveSettings.CollisionRadius);
	}

	if (ProjectileMovementComponent)
	{
		ProjectileMovementComponent->ProjectileGravityScale = ActiveSettings.GravityScale;
		ProjectileMovementComponent->bShouldBounce = ActiveSettings.bShouldBounce;
		ProjectileMovementComponent->Bounciness = ActiveSettings.Bounciness;
		ProjectileMovementComponent->Friction = ActiveSettings.Friction;
	}
}

void ATimeThiefThrowableProjectile::StartGrenadeTrail()
{
	if (!WeaponTrail || !CollisionComponent)
	{
		return;
	}

	WeaponTrail->StopProjectileTrail(ActiveTrailComponent);
	ActiveTrailComponent = WeaponTrail->StartProjectileTrail(ETimeThiefWeaponTrailType::Grenade, *CollisionComponent);
}
