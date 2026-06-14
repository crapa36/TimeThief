#include "Weapon/TimeThiefRocketProjectile.h"
#include "Components/AudioComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/Skill/TimeThiefSkillComponent.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "Network/NetworkGameInstanceSubsystem.h"
#include "Network/State/NetworkEntityState.h"
#include "Smoke/TimeThiefSmokeWorldSubsystem.h"
#include "UObject/ConstructorHelpers.h"

ATimeThiefRocketProjectile::ATimeThiefRocketProjectile()
{
	// PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bCanEverTick = true;	// Network 보간을 위해서 필요하다고 판단
	PrimaryActorTick.bStartWithTickEnabled = false;
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

	FlightLoopAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("FlightLoopAudioComponent"));
	FlightLoopAudioComponent->SetupAttachment(CollisionComponent);
	FlightLoopAudioComponent->bAutoActivate = false;

	ProjectileNiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("ProjectileNiagaraComponent"));
	ProjectileNiagaraComponent->SetupAttachment(CollisionComponent);
	ProjectileNiagaraComponent->SetAutoActivate(false);
	ProjectileNiagaraComponent->SetHiddenInGame(true);

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> DefaultProjectileNiagaraEffect(
		TEXT("/Game/Assets/GrenadeEMP_vfx/Niagara/EMP_Basic/NS_Projectile_EMP.NS_Projectile_EMP"));
	if (DefaultProjectileNiagaraEffect.Succeeded())
	{
		ProjectileNiagaraEffect = DefaultProjectileNiagaraEffect.Object;
		ProjectileNiagaraComponent->SetAsset(ProjectileNiagaraEffect);
	}

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> DefaultExplosionNiagaraEffect(
		TEXT("/Game/Assets/GrenadeEMP_vfx/Niagara/EMP_BigSize/NS_EMP_Big.NS_EMP_Big"));
	if (DefaultExplosionNiagaraEffect.Succeeded())
	{
		ExplosionNiagaraEffect = DefaultExplosionNiagaraEffect.Object;
	}
}

void ATimeThiefRocketProjectile::InitializeProjectileSettings(float InInitSpeed, float InExplosionRadius)
{
	InitialSpeed = InInitSpeed;
	MaxSpeed = InInitSpeed;
	ExplosionRadius = InExplosionRadius;
	
	if (ProjectileMovementComponent)
	{
		ProjectileMovementComponent->InitialSpeed = InitialSpeed;
		ProjectileMovementComponent->MaxSpeed = MaxSpeed;
	}
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
	bHasNetworkTargetLocation = false;
	SetActorTickEnabled(false);

	ActivateProjectileNiagara();

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
	StopProjectileNiagara();
	DeactivateLegacyTrailComponents();
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
	
	SetActorTickEnabled(false);
	bHasNetworkTargetLocation = false;
}

void ATimeThiefRocketProjectile::ExplodeSyncNetwork(const FVector& ExplosionLocation)
{
#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Warning, TEXT("[Rocket] Explode"));
#endif
	
	// TODO: 재현을 위해 정교한 Normal 값이 필요하다면 패킷에 포함 시키는 것도 고려해야 한다
	PlayExplosionEffects(ExplosionLocation, FVector::UpVector);

	if (UTimeThiefSmokeWorldSubsystem* SmokeSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UTimeThiefSmokeWorldSubsystem>() : nullptr)
	{
		SmokeSubsystem->SubmitExplosion(ExplosionLocation, ExplosionRadius, 1.0f, FMath::Rand());
	}

	if (UWorld* World = GetWorld(); ExplosionDebugDuration > 0.0f && ExplosionDebugSegments >= 4)
	{
		DrawDebugSphere(World, ExplosionLocation, ExplosionRadius, ExplosionDebugSegments, FColor::Red, false, ExplosionDebugDuration, 0, 1.5f);
		DrawDebugSphere(World, ExplosionLocation, DamageInnerRadius, ExplosionDebugSegments, FColor::Yellow, false, ExplosionDebugDuration, 0, 1.0f);
	}

	DeactivateProjectile();
}

void ATimeThiefRocketProjectile::BeginPlay()
{
	Super::BeginPlay();
	SetActorHiddenInGame(true);
	DeactivateLegacyTrailComponents();

	if (CollisionComponent)
	{
		CollisionComponent->OnComponentHit.AddDynamic(this, &ATimeThiefRocketProjectile::OnProjectileHit);
	}

	if (auto* NGIS = UNetworkGameInstanceSubsystem::Get(this))
	{
		if (NGIS->IsConnected())
		{
			// 네트워크 연결된 상태에서는 서버에서 폭발 처리하므로 클라이언트에서의 충돌은 무시한다
			CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			return;
		}
	}
	
	if (ProjectileMovementComponent)
	{
		ProjectileMovementComponent->OnProjectileStop.AddDynamic(this, &ATimeThiefRocketProjectile::ExplodeOnce);
	}
}

void ATimeThiefRocketProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopProjectileNiagara();
	DeactivateLegacyTrailComponents();

	Super::EndPlay(EndPlayReason);
}

void ATimeThiefRocketProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	if (!bHasNetworkTargetLocation)
	{
		SetActorTickEnabled(false);
		return;
	}

	if (bHasNetworkTargetLocation)
	{
		const FVector CurrentLocation = GetActorLocation();

		const FVector NewLocation = FMath::VInterpTo(
			CurrentLocation,
			NetworkTargetLocation,
			DeltaTime,
			12.0f);

		SetActorLocation(NewLocation, false, nullptr, ETeleportType::ResetPhysics);

		if (FVector::DistSquared(NewLocation, NetworkTargetLocation) < FMath::Square(5.0f))
		{
			bHasNetworkTargetLocation = false;
			SetActorTickEnabled(false);
		}
	}
}

void ATimeThiefRocketProjectile::OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (auto* NGIS = UNetworkGameInstanceSubsystem::Get(this))
	{
		if (NGIS->IsConnected())
		{
			// 네트워크 연결된 상태에서는 서버에서 폭발 처리하므로 클라이언트에서의 충돌은 무시한다
			return;
		}
	}
	
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

	if (auto* NGIS = UNetworkGameInstanceSubsystem::Get(this))
	{
		// Damage 처리는 서버에서만 진행한다 (현재 서버에서 연결되어 테스트 하는 상황이 아니라면 Damage 직접 적용)
		if (!NGIS->IsConnected())
		{
			ApplyExplosionDamage(ExplosionLocation);
		}
	}
	PlayExplosionEffects(ExplosionLocation, ExplosionNormal);

	if (UTimeThiefSmokeWorldSubsystem* SmokeSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UTimeThiefSmokeWorldSubsystem>() : nullptr)
	{
		SmokeSubsystem->SubmitExplosion(ExplosionLocation, ExplosionRadius, 1.0f, FMath::Rand());
	}

	if (UWorld* World = GetWorld(); ExplosionDebugDuration > 0.0f && ExplosionDebugSegments >= 4)
	{
		DrawDebugSphere(World, ExplosionLocation, ExplosionRadius, ExplosionDebugSegments, FColor::Red, false, ExplosionDebugDuration, 0, 1.5f);
		DrawDebugSphere(World, ExplosionLocation, DamageInnerRadius, ExplosionDebugSegments, FColor::Yellow, false, ExplosionDebugDuration, 0, 1.0f);
	}

	DeactivateProjectile();
}

void ATimeThiefRocketProjectile::ApplyExplosionDamage(const FVector& ExplosionLocation)
{
	if (!HasAuthority())
	{
		return;
	}

	float DamageMultiplier = 1.0f;
	if (AActor* OwnerActorForSkill = CachedOwnerActor.Get())
	{
		if (const UTimeThiefSkillComponent* SkillComponent = OwnerActorForSkill->FindComponentByClass<UTimeThiefSkillComponent>())
		{
			DamageMultiplier = SkillComponent->GetDamageMultiplier();
		}
	}

	const float FinalMaxDamage = (MaxDamage + DamageBonus) * DamageMultiplier;
	const float FinalMinDamage = (MinDamage + DamageBonus) * DamageMultiplier;

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

	if (ExplosionNiagaraEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ExplosionNiagaraEffect, ExplosionLocation, ExplosionRotation);
	}

	if (ExplosionSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ExplosionSound, ExplosionLocation);
	}
}

void ATimeThiefRocketProjectile::ActivateProjectileNiagara()
{
	DeactivateLegacyTrailComponents();

	if (!ProjectileNiagaraComponent)
	{
		return;
	}

	if (ProjectileNiagaraEffect)
	{
		ProjectileNiagaraComponent->SetAsset(ProjectileNiagaraEffect);
		ProjectileNiagaraComponent->SetHiddenInGame(false);
		ProjectileNiagaraComponent->Activate(true);

		if (ProjectileMeshComponent)
		{
			ProjectileMeshComponent->SetHiddenInGame(true);
			ProjectileMeshComponent->SetVisibility(false, true);
		}
	}
	else if (ProjectileMeshComponent)
	{
		ProjectileMeshComponent->SetHiddenInGame(false);
		ProjectileMeshComponent->SetVisibility(true, true);
	}
}

void ATimeThiefRocketProjectile::StopProjectileNiagara()
{
	if (ProjectileNiagaraComponent)
	{
		ProjectileNiagaraComponent->Deactivate();
		ProjectileNiagaraComponent->SetHiddenInGame(true);
	}
}

void ATimeThiefRocketProjectile::DeactivateLegacyTrailComponents()
{
	static const FName LegacyTrailComponentName(TEXT("TrailEffectComponent"));

	TArray<UParticleSystemComponent*> ParticleComponents;
	GetComponents(ParticleComponents);

	for (UParticleSystemComponent* ParticleComponent : ParticleComponents)
	{
		if (!ParticleComponent)
		{
			continue;
		}

		if (ParticleComponent->GetFName() == LegacyTrailComponentName)
		{
			ParticleComponent->SetAutoActivate(false);
			ParticleComponent->DeactivateSystem();
			ParticleComponent->SetHiddenInGame(true);
			ParticleComponent->SetVisibility(false, true);
		}
	}
}

void ATimeThiefRocketProjectile::ActivateProjectileFromNetwork(const FVector& SpawnLocation,
	const FVector& InitialVelocity)
{
#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Warning, TEXT("[Rocket] Active"));
#endif
	
	SetActorLocation(SpawnLocation, false, nullptr, ETeleportType::ResetPhysics);
	
	const FVector Direction = InitialVelocity.GetSafeNormal();
	
	if (!Direction.IsNearlyZero())
	{
		SetActorRotation(Direction.Rotation());
	}
	
	bIsActivated = true;
	bExploded = false;
	bHasNetworkTargetLocation = false;
	SetActorTickEnabled(false);

	SetActorHiddenInGame(false);
	
	if (CollisionComponent)
	{
		CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
	
	if (ProjectileMovementComponent)
	{
		ProjectileMovementComponent->SetUpdatedComponent(CollisionComponent);
		ProjectileMovementComponent->StopMovementImmediately();
		ProjectileMovementComponent->Velocity = InitialVelocity;
		ProjectileMovementComponent->Activate(true);
	}

	ActivateProjectileNiagara();
	
	if (FlightLoopAudioComponent && FlightLoopSound)
	{
		FlightLoopAudioComponent->Play();
	}
}

void ATimeThiefRocketProjectile::ApplyNetworkMovementState(const FNetworkEntityState& EntityState)
{
	const FVector ServerLocation = EntityState.Position;
	const FVector ServerVelocity = EntityState.Velocity;
	
	const FVector CurrentLocation = GetActorLocation();
	const float ErrorDistance = FVector::Distance(CurrentLocation, ServerLocation);
	
	// 방향 갱신
	if (!ServerVelocity.IsNearlyZero())
	{
		const FVector ServerDirection = ServerVelocity.GetSafeNormal();
		SetActorRotation(ServerDirection.Rotation());
	}
	
	if (ProjectileMovementComponent)
	{
		ProjectileMovementComponent->Velocity = ServerVelocity;
	}
	
	constexpr const float TeleportThreshold = 300.0f; // 텔레포트 여부 판단 기준 거리 (조정 가능)
	constexpr const float SmoothThreshold = 30.0f; // 부드러운 보정 여부 판단 기준 거리 (조정 가능)
	
	if (ErrorDistance >= TeleportThreshold)
		// 서버와의 위치 오차가 너무 크면 텔레포트로 보정
	{
		SetActorLocation(ServerLocation, false, nullptr, ETeleportType::ResetPhysics);
		bHasNetworkTargetLocation = false;
		SetActorTickEnabled(false);
	}
	else if (ErrorDistance >= SmoothThreshold)
		// 서버와의 위치 오차가 어느 정도 있지만 텔레포트할 정도는 아니면 부드럽게 보정
	{
		NetworkTargetLocation = ServerLocation;
		bHasNetworkTargetLocation = true;
		SetActorTickEnabled(true);
	}
	else
	{
		bHasNetworkTargetLocation = false;
		SetActorTickEnabled(false);
	}
}
