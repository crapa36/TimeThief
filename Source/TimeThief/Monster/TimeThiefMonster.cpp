


#include "TimeThiefMonster.h"

#include "Network/NetworkCombatSyncComponent.h"
#include "Network/NetworkEntityComponent.h"
#include "Network/NetworkMoveComponent.h"


// Sets default values
ATimeThiefMonster::ATimeThiefMonster()
{
	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	SceneRootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRootComponent"));
	SetRootComponent(SceneRootComponent);

	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleComponent"));
	CapsuleComponent->SetupAttachment(SceneRootComponent);

	MeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(SceneRootComponent);

	MonsterCombatComponent = CreateDefaultSubobject<UTimeThiefMonsterCombatComponent>(TEXT("MonsterCombatComponent"));
	
	NetworkEntityComponent = CreateDefaultSubobject<UNetworkEntityComponent>(TEXT("NetworkEntityComponent"));
	NetworkMoveComponent = CreateDefaultSubobject<UNetworkMoveComponent>(TEXT("NetworkMoveComponent"));
	NetworkCombatSyncComponent = CreateDefaultSubobject<UNetworkCombatSyncComponent>(TEXT("NetworkCombatSyncComponent"));
}

// Called when the game starts or when spawned
void ATimeThiefMonster::BeginPlay()
{
	Super::BeginPlay();
	
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

void ATimeThiefMonster::HandleRemoteAttackRequest_Implementation(const FRemoteAttackNotify& AttackRequest)
{
}

