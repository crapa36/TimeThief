


#include "TimeThiefMonster.h"

#include "TimeThiefMonsterAnimInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Network/NetworkCombatSyncComponent.h"
#include "Network/NetworkEntityComponent.h"
#include "Network/NetworkMoveComponent.h"


// Sets default values
ATimeThiefMonster::ATimeThiefMonster()
{
	PrimaryActorTick.bCanEverTick = false;
	
	SceneRootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRootComponent"));
	SetRootComponent(SceneRootComponent);

	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleComponent"));
	CapsuleComponent->SetupAttachment(SceneRootComponent);

	MeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(SceneRootComponent);
	
	MeshComponent->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	MeshComponent->SetAnimInstanceClass(UTimeThiefMonsterAnimInstance::StaticClass());

	MonsterCombatComponent = CreateDefaultSubobject<UTimeThiefMonsterCombatComponent>(TEXT("MonsterCombatComponent"));
	
	NetworkEntityComponent = CreateDefaultSubobject<UNetworkEntityComponent>(TEXT("NetworkEntityComponent"));
	NetworkMoveComponent = CreateDefaultSubobject<UNetworkMoveComponent>(TEXT("NetworkMoveComponent"));
	NetworkCombatSyncComponent = CreateDefaultSubobject<UNetworkCombatSyncComponent>(TEXT("NetworkCombatSyncComponent"));
	
	DissolveFXComponent = CreateDefaultSubobject<UTimeThiefDissolveFXComponent>(TEXT("DissolveFXComponent"));
}

// Called when the game starts or when spawned
void ATimeThiefMonster::BeginPlay()
{
	Super::BeginPlay();
	
	// FX Component가 자동 수집하게
	// if (DissolveFXComponent && MeshComponent)
	// {
	// 	DissolveFXComponent->RegisterMesh(MeshComponent);
	// }
}

// Called every frame
void ATimeThiefMonster::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Called to bind functionality to input
void ATimeThiefMonster::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

UNetworkEntityComponent* ATimeThiefMonster::GetNetworkEntityComponent() const
{
	return NetworkEntityComponent;
}

FVector ATimeThiefMonster::GetNetworkLocation() const
{
	return GetActorLocation();
}

void ATimeThiefMonster::SetNetworkLocation(const FVector& NewLocation)
{
	SetActorLocation(NewLocation);
}

float ATimeThiefMonster::GetNetworkCharYaw() const
{
	return GetActorRotation().Yaw;
}

void ATimeThiefMonster::SetNetworkCharYaw(float NewCharYaw)
{
	FRotator NewRotation = GetActorRotation();
	NewRotation.Yaw = FRotator::NormalizeAxis(NewCharYaw);
	SetActorRotation(NewRotation);
}

float ATimeThiefMonster::GetNetworkAimYaw() const
{
	return 0.0f;
}

void ATimeThiefMonster::SetNetworkAimYaw(float NewAimYaw)
{
}

float ATimeThiefMonster::GetNetworkAimPitch() const
{
	return 0.0f;
}

void ATimeThiefMonster::SetNetworkAimPitch(float NewAimPitch)
{
}

FVector2D ATimeThiefMonster::GetNetworkVelocity2D() const
{
	return CurrentNetworkVelocity;
}

void ATimeThiefMonster::SetNetworkVelocity2D(FVector2D NewVelocity)
{
	CurrentNetworkVelocity = NewVelocity;
}

EMovementMode ATimeThiefMonster::GetNetworkMovementMode() const
{
	return MOVE_None;
}

void ATimeThiefMonster::SetNetworkMovementMode(EMovementMode NewMovementMode)
{
}

float ATimeThiefMonster::GetLocalControlAimYaw() const
{
	return 0.0f;
}

float ATimeThiefMonster::GetLocalControlAimPitch() const
{
	return 0.0f;
}

FVector2D ATimeThiefMonster::GetLocalControlVelocity2D() const
{
	return FVector2D::ZeroVector;
}

EMovementMode ATimeThiefMonster::GetLocalControlMovementMode() const
{
	return EMovementMode::MOVE_None;
}

FVector ATimeThiefMonster::GetMoveStep() const
{
	if (NetworkMoveComponent == nullptr)
	{
		return FVector::ZeroVector;
	}
	
	return NetworkMoveComponent->GetMoveStep();
}

void ATimeThiefMonster::ApplyNetworkMovementState(const FNetworkEntityState& EntityState)
{
	if (NetworkMoveComponent == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[NetworkEntity] NetworkMoveComponent is nullptr"));
		return;
	}
	
	NetworkMoveComponent->ApplyNetworkState(EntityState);
}

float ATimeThiefMonster::GetAimYaw() const
{
	if (!TargetActor.IsValid())
	{
		return 0.0f;
	}

	const FVector MyLocation = GetActorLocation();
	const FVector TargetLocation = TargetActor->GetActorLocation();

	const FRotator LookAtRot = (TargetLocation - MyLocation).Rotation();
	const FRotator Delta = UKismetMathLibrary::NormalizedDeltaRotator(LookAtRot, GetActorRotation());

	return FMath::Clamp(Delta.Yaw, -75.0f, 75.0f);
}

float ATimeThiefMonster::GetAimPitch() const
{
	if (!TargetActor.IsValid())
	{
		return 0.0f;
	}

	const FVector MyLocation = GetActorLocation();
	const FVector TargetLocation = TargetActor->GetActorLocation();

	const FRotator LookAtRot = (TargetLocation - MyLocation).Rotation();
	const FRotator Delta = UKismetMathLibrary::NormalizedDeltaRotator(LookAtRot, GetActorRotation());

	return FMath::Clamp(Delta.Pitch, -50.0f, 50.0f);
}

bool ATimeThiefMonster::IsDead() const
{
	return bIsDead;
}

class UTimeThiefPawnCombatComponent* ATimeThiefMonster::GetCombatComponent() const
{
	return MonsterCombatComponent;
}

class UNetworkCombatSyncComponent* ATimeThiefMonster::GetCombatSyncComponent() const
{
	return NetworkCombatSyncComponent;
}

uint32 ATimeThiefMonster::GetCombatEntityId() const
{
	return GetEntityId();
}

uint32 ATimeThiefMonster::GetEntityId() const
{
	return NetworkEntityComponent ? NetworkEntityComponent->GetEntityId() : 0;
}

void ATimeThiefMonster::SetTarget(uint32 InTargetId, AActor* InTargetActor)
{
	TargetId = InTargetId;
	TargetActor = InTargetActor;
}

void ATimeThiefMonster::HandleRemoteCombatRequest(const FRemoteAttackNotify& AttackRequest)
{
	RemoteCombat(AttackRequest);
}

void ATimeThiefMonster::OnDeathNetwork()
{
	if (bIsDead)
	{
		UE_LOG(LogTemp, Warning, TEXT("[NetworkEntity] Already Dead"));
		return;
	}
	
	bIsDead = true;
	VisualState = EMonsterVisualState::Dying;
	
	SetActorHiddenInGame(false);
	DisableCombatCollision();
	StopMovementVisual();
	
	if (MeshComponent)
	{
		if (UAnimInstance* AnimInst = MeshComponent->GetAnimInstance())
		{
			if (DeathMontage)
			{
				AnimInst->Montage_Play(DeathMontage);
				
				GetWorldTimerManager().SetTimer(
					DeathHideTimerHandle,
					this,
					&ATimeThiefMonster::StartDeathDisappearEffect,
					DeathMontage->GetPlayLength(),
					false
				);
				
				// FOnMontageEnded EndDelegate;
				// EndDelegate.BindUObject(this, &ATimeThiefMonster::OnDeathMontageEnded);
				// AnimInst->Montage_SetEndDelegate(EndDelegate, DeathMontage);
				return;
			}
		}
	}
	
	OnDeathMontageFinishedFallback();
}

void ATimeThiefMonster::OnRespawnNetwork(const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	GetWorldTimerManager().ClearTimer(DeathHideTimerHandle);
	
	if (MeshComponent)
	{
		if (UAnimInstance* AnimInst = MeshComponent->GetAnimInstance())
		{
			if (DeathMontage)
			{
				AnimInst->Montage_Stop(0.0f, DeathMontage);
			}
		}
	}

	SetActorLocation(SpawnLocation);
	SetActorRotation(SpawnRotation);
	
	if (NetworkMoveComponent)
	{
		NetworkMoveComponent->ResetInterpolationToCurrent();
		NetworkMoveComponent->ResumeVisualMovement();
	}

	bIsDead = false;
	VisualState = EMonsterVisualState::Respawning;

	SetActorHiddenInGame(false);
	SetActorTickEnabled(true);
	SetActorEnableCollision(false);

	if (MeshComponent)
	{
		MeshComponent->SetVisibility(true, true);
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	PlayRespawnEffect();

	GetWorldTimerManager().SetTimer(
		RespawnFinishTimerHandle,
		this,
		&ATimeThiefMonster::FinishRespawn,
		0.8f,		// TODO: 이거 하드 코딩 제외 해야 함
		false
	);
}

void ATimeThiefMonster::RemoteCombat(const FRemoteAttackNotify& AttackNotify)
{
	if (bIsDead || VisualState != EMonsterVisualState::Alive)
	{
		return;
	}
	
	switch (AttackNotify.NotifyType)
	{
		case ECombatNotifyType::Fire:
			RemoteFire(AttackNotify);
			break;
		case ECombatNotifyType::Attack:
			RemoteAttack(AttackNotify);
			break;
		
		case ECombatNotifyType::Hit:
			RemoteHit(AttackNotify);
			break;
		
		case ECombatNotifyType::Cancel:
			RemoteCancelAttack(AttackNotify);
			break;
		
		default:
			break;
	}
}

void ATimeThiefMonster::RemoteFire(const FRemoteAttackNotify& AttackNotify)
{
	const FVector MuzzleLocation = AttackNotify.Origin;
	const FVector FireDirection = AttackNotify.Direction;
	const float FireRange = AttackNotify.Range;
	
	const FVector EndLocation = MuzzleLocation + FireDirection * FireRange;
	
	// Ray / Beam FX
	if (FireCastFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			FireCastFX,
			MuzzleLocation,
			FireDirection.Rotation()
		);
	}
	
	// 사운드 재생
	if (FireSound)
	{
		if (MonsterSoundAttenuation)
		{
			UGameplayStatics::SpawnSoundAtLocation(
				this,
				FireSound,
				GetActorLocation(),
				GetActorRotation(),
				1.0f,
				1.0f,
				0.0f,
				MonsterSoundAttenuation);
		}
		else
		{
			UGameplayStatics::SpawnSoundAtLocation(
				this,
				FireSound,
				GetActorLocation(),
				GetActorRotation(),
				1.0f,
				1.0f,
				0.0f);
		}
	}

	// Impact FX
	if (FireImpactFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			FireImpactFX,
			MuzzleLocation,
			FireDirection.Rotation()
		);
	}
}

void ATimeThiefMonster::RemoteAttack(const FRemoteAttackNotify& AttackNotify)
{
	UAnimMontage* Montage = GetAttackMontage(AttackNotify.AttackId);
	if (!Montage)
		return;
	
	CurrentAttackMontage = Montage;
	if (MeshComponent)
	{
		if (UAnimInstance* AnimInst = MeshComponent->GetAnimInstance())
		{
			AnimInst->Montage_Play(Montage);
		}
	}
}

void ATimeThiefMonster::RemoteHit(const FRemoteAttackNotify& AttackNotify)
{
	// TODO: Hit Animation 재생, Hit Effect, 사운드 등등
	
	if (!HitReactMontage)
		return;

	if (!MeshComponent)
		return;

	UAnimInstance* AnimInst = MeshComponent->GetAnimInstance();
	if (!AnimInst)
		return;

	AnimInst->Montage_Play(HitReactMontage, 1.0f);
}

void ATimeThiefMonster::RemoteCancelAttack(const FRemoteAttackNotify& AttackNotify)
{
	// TODO: Cancel Montage
	if (MeshComponent)
	{
		if (UAnimInstance* AnimInst = MeshComponent->GetAnimInstance())
		{
			if (CurrentAttackMontage)
			{
				AnimInst->Montage_Stop(0.15f, CurrentAttackMontage);
				CurrentAttackMontage = nullptr;
			}
		}
	}
}

UAnimMontage* ATimeThiefMonster::GetAttackMontage(int32 AttackType) const
{
	if (const TObjectPtr<UAnimMontage>* Found = AttackMontageMap.Find(AttackType))
	{
		return Found->Get();
	}
	return nullptr;
}

void ATimeThiefMonster::StartDeathDisappearEffect()
{
	if (VisualState == EMonsterVisualState::DeadHidden)
	{
		return;
	}

	VisualState = EMonsterVisualState::Dying;

	if (DissolveFXComponent)
	{
		DissolveFXComponent->PlayDisappear();
	}
	
	GetWorldTimerManager().SetTimer(
		DeathHideTimerHandle,
		this,
		&ATimeThiefMonster::FinishDeathHide,
		2.0f,		// TODO: 이거 하드 코딩 제외 해야 함
		false
		);
}

void ATimeThiefMonster::FinishDeathHide()
{
	VisualState = EMonsterVisualState::DeadHidden;

	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);

	if (MeshComponent)
	{
		MeshComponent->SetVisibility(false, true);
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// Tick도 꺼도 됨. 단, Respawn 패킷을 Actor가 직접 받는 구조면 Actor 자체 Disable은 조심.
}

void ATimeThiefMonster::PlayRespawnEffect()
{
	if (DissolveFXComponent)
	{
		DissolveFXComponent->PlayAppear();
	}
}

void ATimeThiefMonster::DisableCombatCollision()
{
	SetActorEnableCollision(false);

	if (CapsuleComponent)
	{
		CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		CapsuleComponent->SetGenerateOverlapEvents(false);
	}

	if (MeshComponent)
	{
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void ATimeThiefMonster::EnableCombatCollision()
{
	// 가장 단순한 복구.
	
	if (CapsuleComponent)
	{
		CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		CapsuleComponent->SetCollisionResponseToAllChannels(ECR_Block);
		CapsuleComponent->SetGenerateOverlapEvents(true);
	}

	if (MeshComponent)
	{
		// 보통 캐릭터 Mesh는 충돌을 안 쓰고 Capsule만 씀.
		// 필요 없다면 NoCollision 유지 추천.
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void ATimeThiefMonster::StopMovementVisual()
{
	if (NetworkMoveComponent)
	{
		NetworkMoveComponent->StopVisualMovement();
	}
}

void ATimeThiefMonster::OnDeathMontageFinishedFallback()
{
	StartDeathDisappearEffect();
}

void ATimeThiefMonster::FinishRespawn()
{
	VisualState = EMonsterVisualState::Alive;
	bIsDead = false;
	
	SetActorHiddenInGame(false);
	SetActorTickEnabled(true);
	SetActorEnableCollision(true);

	if (MeshComponent)
	{
		MeshComponent->SetVisibility(true, true);
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	EnableCombatCollision();
}

void ATimeThiefMonster::OnDeathMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != DeathMontage)
	{
		return;
	}
	
	StartDeathDisappearEffect();
}
