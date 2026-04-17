#include "Weapon/TimeThiefRocketProjectile.h"
#include "Components/AudioComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"

ATimeThiefRocketProjectile::ATimeThiefRocketProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bExploded = true;
	bIsActivated = false;

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	SetRootComponent(CollisionComponent);
	CollisionComponent->InitSphereRadius(CollisionRadius);
	CollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Block);
	CollisionComponent->SetNotifyRigidBodyCollision(true);

	ProjectileMovementComponent = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovementComponent"));
	ProjectileMovementComponent->InitialSpeed = InitialSpeed;
	ProjectileMovementComponent->MaxSpeed = MaxSpeed;
	ProjectileMovementComponent->ProjectileGravityScale = GravityScale;
	ProjectileMovementComponent->bRotationFollowsVelocity = true;
	ProjectileMovementComponent->bAutoActivate = false;

	ProjectileMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMeshComponent"));
	ProjectileMeshComponent->SetupAttachment(CollisionComponent);
	ProjectileMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ProjectileMeshComponent->SetGenerateOverlapEvents(false);

	TrailEffectComponent = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("TrailEffectComponent"));
	TrailEffectComponent->SetupAttachment(CollisionComponent);

	FlightLoopAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("FlightLoopAudioComponent"));
	FlightLoopAudioComponent->SetupAttachment(CollisionComponent);
	FlightLoopAudioComponent->bAutoActivate = false;
}

void ATimeThiefRocketProjectile::InitializeProjectile(AActor* InOwnerActor, APawn* InInstigatorPawn)
{
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

void ATimeThiefRocketProjectile::ActivateProjectile(const FTransform& SpawnTransform)
{
	SetActorTransform(SpawnTransform, false, nullptr, ETeleportType::ResetPhysics);
	bIsActivated = true;
	bExploded = false;
	SetActorHiddenInGame(false);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ProjectileMovementComponent->SetUpdatedComponent(CollisionComponent);
	ProjectileMovementComponent->Velocity = SpawnTransform.GetRotation().GetForwardVector() * InitialSpeed;
	ProjectileMovementComponent->Activate();

	if (FlightLoopAudioComponent && FlightLoopSound)
	{
		FlightLoopAudioComponent->Play();
	}

	if (UWorld* World = GetWorld())
	{
		if (MaxLifeTime > 0.0f)
		{
			World->GetTimerManager().SetTimer(LifeTimeTimerHandle, this, &ATimeThiefRocketProjectile::HandleLifeTimeExpired, MaxLifeTime, false);
		}
	}
}

void ATimeThiefRocketProjectile::DeactivateProjectile()
{
	bIsActivated = false;
	bExploded = true;
	SetActorHiddenInGame(true);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ProjectileMovementComponent->StopMovementImmediately();
	ProjectileMovementComponent->Deactivate();

	if (FlightLoopAudioComponent)
	{
		FlightLoopAudioComponent->Stop();
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LifeTimeTimerHandle);
	}
}

void ATimeThiefRocketProjectile::BeginPlay()
{
	Super::BeginPlay();
	SetActorHiddenInGame(true);

	if (CollisionComponent)
	{
		CollisionComponent->OnComponentHit.AddDynamic(this, &ATimeThiefRocketProjectile::OnProjectileHit);
	}

	if (ProjectileMovementComponent)
	{
		ProjectileMovementComponent->OnProjectileStop.AddDynamic(this, &ATimeThiefRocketProjectile::ExplodeOnce);
	}
}

void ATimeThiefRocketProjectile::OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (OtherActor && (OtherActor == CachedOwnerActor.Get() || OtherActor == CachedInstigatorPawn.Get()))
	{
		return;
	}

	ExplodeOnce(Hit);
}

void ATimeThiefRocketProjectile::HandleLifeTimeExpired()
{
	FHitResult EmptyHit;
	EmptyHit.ImpactPoint = GetActorLocation();
	EmptyHit.ImpactNormal = FVector::UpVector;
	ExplodeOnce(EmptyHit);
}

void ATimeThiefRocketProjectile::ExplodeOnce(const FHitResult& Hit)
{
	if (!bIsActivated || bExploded)
	{
		return;
	}

	// OnHit and OnProjectileStop may fire close together; lock immediately.
	bExploded = true;
	const FVector ExplosionLocation = Hit.bBlockingHit ? FVector(Hit.ImpactPoint) : GetActorLocation();
	const FVector ExplosionNormal = Hit.bBlockingHit ? FVector(Hit.ImpactNormal) : FVector::UpVector;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LifeTimeTimerHandle);
	}

	ApplyExplosionDamage(ExplosionLocation);
	PlayExplosionEffects(ExplosionLocation, ExplosionNormal);

	if (UWorld* World = GetWorld())
	{
		const int32 Segments = FMath::Max(4, ExplosionDebugSegments);
		DrawDebugSphere(World, ExplosionLocation, ExplosionRadius, Segments, FColor::Red, false, ExplosionDebugDuration, 0, 1.5f);
		DrawDebugSphere(World, ExplosionLocation, DamageInnerRadius, Segments, FColor::Yellow, false, ExplosionDebugDuration, 0, 1.0f);
	}

	DeactivateProjectile();
}

void ATimeThiefRocketProjectile::ApplyExplosionDamage(const FVector& ExplosionLocation)
{
	if (!HasAuthority())
	{
		return;
	}

	const float FinalMaxDamage = MaxDamage + DamageBonus;
	const float FinalMinDamage = MinDamage + DamageBonus;

	AController* InstigatorController = nullptr;
	if (APawn* InstigatorPawn = CachedInstigatorPawn.Get())
	{
		InstigatorController = InstigatorPawn->GetController();
	}

	TArray<AActor*> IgnoreActors;
	IgnoreActors.Add(this);
	AActor* OwnerActor = CachedOwnerActor.Get();
	if (OwnerActor)
	{
		IgnoreActors.Add(OwnerActor);
	}

	UGameplayStatics::ApplyRadialDamageWithFalloff(
		this,
		FinalMaxDamage,
		FinalMinDamage,
		ExplosionLocation,
		DamageInnerRadius,
		ExplosionRadius,
		1.0f,
		nullptr,
		IgnoreActors,
		this,
		InstigatorController,
		ECC_Visibility
	);

	if (OwnerActor)
	{
		const float SafeInnerRadius = FMath::Min(DamageInnerRadius, ExplosionRadius);
		const float DistanceToOwner = FVector::Distance(ExplosionLocation, OwnerActor->GetActorLocation());
		if (DistanceToOwner <= ExplosionRadius)
		{
			float OwnerDamage = FinalMaxDamage;
			if (DistanceToOwner > SafeInnerRadius)
			{
				const float RadiusSpan = FMath::Max(1.0f, ExplosionRadius - SafeInnerRadius);
				const float FalloffAlpha = FMath::Clamp((DistanceToOwner - SafeInnerRadius) / RadiusSpan, 0.0f, 1.0f);
				OwnerDamage = FMath::Lerp(FinalMaxDamage, FinalMinDamage, FalloffAlpha);
			}

			const float OwnerDamageScale = FMath::Max(SelfDamageScale, 0.01f);
			const float FinalOwnerDamage = FMath::Max(1.0f, OwnerDamage * OwnerDamageScale);
			UGameplayStatics::ApplyDamage(OwnerActor, FinalOwnerDamage, InstigatorController, this, nullptr);
		}
	}
}

void ATimeThiefRocketProjectile::PlayExplosionEffects(const FVector& ExplosionLocation, const FVector& ExplosionNormal)
{
	const FRotator ExplosionRotation = FRotationMatrix::MakeFromZ(ExplosionNormal.GetSafeNormal()).Rotator();

	if (ExplosionEffect)
	{
		UGameplayStatics::SpawnEmitterAtLocation(this, ExplosionEffect, ExplosionLocation, ExplosionRotation);
	}

	if (ExplosionSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ExplosionSound, ExplosionLocation);
	}
}