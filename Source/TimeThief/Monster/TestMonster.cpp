


#include "TestMonster.h"

#include "Network/NetworkCombatSyncComponent.h"
#include "Network/NetworkEntityComponent.h"
#include "Network/NetworkMoveComponent.h"
#include "Network/State/NetworkEntityState.h"


// Sets default values
ATestMonster::ATestMonster()
{
	PrimaryActorTick.bCanEverTick = false;
	
	NetworkEntityComponent = CreateDefaultSubobject<UNetworkEntityComponent>(TEXT("NetworkEntityComponent"));
	NetworkMoveComponent = CreateDefaultSubobject<UNetworkMoveComponent>(TEXT("NetworkMoveComponent"));
	NetworkCombatSyncComponent = CreateDefaultSubobject<UNetworkCombatSyncComponent>(TEXT("NetworkCombatSyncComponent"));
}

// Called when the game starts or when spawned
void ATestMonster::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ATestMonster::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Called to bind functionality to input
void ATestMonster::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

UNetworkEntityComponent* ATestMonster::GetNetworkEntityComponent() const
{
	return NetworkEntityComponent;
}

FVector ATestMonster::GetNetworkLocation() const
{
	return GetActorLocation();
}

void ATestMonster::SetNetworkLocation(const FVector& NewLocation)
{
	SetActorLocation(NewLocation);
}

float ATestMonster::GetNetworkCharYaw() const
{
	return GetActorRotation().Yaw;
}

void ATestMonster::SetNetworkCharYaw(float NewCharYaw)
{
	FRotator NewRotation = GetActorRotation();
	NewRotation.Yaw = FRotator::NormalizeAxis(NewCharYaw);
	SetActorRotation(NewRotation);
}

float ATestMonster::GetNetworkAimYaw() const
{
	return 0.0f;
}

void ATestMonster::SetNetworkAimYaw(float NewAimYaw)
{
}

float ATestMonster::GetNetworkAimPitch() const
{
	return 0.0f;
}

void ATestMonster::SetNetworkAimPitch(float NewAimPitch)
{
}

FVector2D ATestMonster::GetNetworkVelocity2D() const
{
	return CurrentNetworkVelocity;
}

void ATestMonster::SetNetworkVelocity2D(FVector2D NewVelocity)
{
	CurrentNetworkVelocity = NewVelocity;
}

EMovementMode ATestMonster::GetNetworkMovementMode() const
{
	return CurrentNetworkMovementMode;
}

void ATestMonster::SetNetworkMovementMode(EMovementMode NewMovementMode)
{
	CurrentNetworkMovementMode = NewMovementMode;
}

float ATestMonster::GetLocalControlAimYaw() const
{
	return 0.0f;
}

float ATestMonster::GetLocalControlAimPitch() const
{
	return 0.0f;
}

FVector2D ATestMonster::GetLocalControlVelocity2D() const
{
	return FVector2D::ZeroVector;
}

EMovementMode ATestMonster::GetLocalControlMovementMode() const
{
	return EMovementMode::MOVE_None;
}

FVector ATestMonster::GetMoveStep() const
{
	if (NetworkMoveComponent == nullptr)
	{
		return FVector::ZeroVector;
	}
	
	return NetworkMoveComponent->GetMoveStep();
}

void ATestMonster::ApplyNetworkMovementState(const FNetworkEntityState& EntityState)
{
	if (NetworkMoveComponent == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[NetworkEntity] NetworkMoveComponent is nullptr"));
		return;
	}
	
	NetworkMoveComponent->ApplyNetworkState(EntityState);
}

class UTimeThiefPawnCombatComponent* ATestMonster::GetCombatComponent() const
{
	return nullptr;
}

class UNetworkCombatSyncComponent* ATestMonster::GetCombatSyncComponent() const
{
	return NetworkCombatSyncComponent;
}

uint32 ATestMonster::GetCombatEntityId() const
{
	return GetEntityId();
}

uint32 ATestMonster::GetEntityId() const
{
	return NetworkEntityComponent ? NetworkEntityComponent->GetEntityId() : 0;
}

