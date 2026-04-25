


#include "TimeThiefNetworkCharacterBase.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "Network/NetworkEntityComponent.h"
#include "Network/NetworkMoveComponent.h"
#include "Network/NetworkCombatSyncComponent.h"


ATimeThiefNetworkCharacterBase::ATimeThiefNetworkCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	
	NetworkEntityComponent = CreateDefaultSubobject<UNetworkEntityComponent>(TEXT("NetworkEntityComponent"));
	NetworkMoveComponent = CreateDefaultSubobject<UNetworkMoveComponent>(TEXT("NetworkMoveComponent"));
	NetworkCombatSyncComponent = CreateDefaultSubobject<UNetworkCombatSyncComponent>(TEXT("NetworkCombatSyncComponent"));
}

ATimeThiefNetworkCharacterBase::~ATimeThiefNetworkCharacterBase()
{
}

// Called when the game starts or when spawned
void ATimeThiefNetworkCharacterBase::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ATimeThiefNetworkCharacterBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

FRotator ATimeThiefNetworkCharacterBase::GetBaseAimRotation() const
{
	return FRotator(CurrentNetworkPitch, GetNetworkYaw(), 0.0f);
}

// Called to bind functionality to input
void ATimeThiefNetworkCharacterBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

UNetworkEntityComponent* ATimeThiefNetworkCharacterBase::GetNetworkEntityComponent() const
{
	return NetworkEntityComponent;
}

FVector ATimeThiefNetworkCharacterBase::GetNetworkLocation() const
{
	return GetActorLocation();
}

void ATimeThiefNetworkCharacterBase::SetNetworkLocation(const FVector& NewLocation)
{
	SetActorLocation(NewLocation);
}

float ATimeThiefNetworkCharacterBase::GetNetworkYaw() const
{
	return GetActorRotation().Yaw;
}

void ATimeThiefNetworkCharacterBase::SetNetworkYaw(float NewYaw)
{
	FRotator NewRotation = GetActorRotation();
	NewRotation.Yaw = FRotator::NormalizeAxis(NewYaw);
	SetActorRotation(NewRotation);
}

float ATimeThiefNetworkCharacterBase::GetNetworkAimYaw() const
{
	return CurrentNetworkAimYaw;
}

void ATimeThiefNetworkCharacterBase::SetNetworkAimYaw(float NewAimYaw)
{
	CurrentNetworkAimYaw = NewAimYaw;
}

float ATimeThiefNetworkCharacterBase::GetNetworkPitch() const
{
	return CurrentNetworkPitch;
}

void ATimeThiefNetworkCharacterBase::SetNetworkPitch(float NewPitch)
{
	const float NormalizedPitch = FRotator::NormalizeAxis(NewPitch);
	CurrentNetworkPitch = FMath::Clamp(NormalizedPitch, -89.0f, 89.0f);
}

FVector2D ATimeThiefNetworkCharacterBase::GetNetworkVelocity2D() const
{
	return CurrentNetworkVelocity;;
}

void ATimeThiefNetworkCharacterBase::SetNetworkVelocity2D(FVector2D NewVelocity)
{
	CurrentNetworkVelocity = NewVelocity;
}

EMovementMode ATimeThiefNetworkCharacterBase::GetNetworkMovementMode() const
{
	return CurrentNetworkMovementMode;
}

void ATimeThiefNetworkCharacterBase::SetNetworkMovementMode(EMovementMode NewMovementMode)
{
	CurrentNetworkMovementMode = NewMovementMode;
}

float ATimeThiefNetworkCharacterBase::GetLocalControlAimYaw() const
{
	return 0.0f;
}

float ATimeThiefNetworkCharacterBase::GetLocalControlPitch() const
{
	const float ActorPitch = FRotator::NormalizeAxis(GetActorRotation().Pitch);
	return FMath::Clamp(ActorPitch, -89.0f, 89.0f);
}

FVector2D ATimeThiefNetworkCharacterBase::GetLocalControlVelocity2D() const
{
	auto CMC = GetCharacterMovement();
	if (CMC == nullptr)
	{
		return FVector2D::ZeroVector;
	}
	
	return FVector2D(CMC->Velocity.X, CMC->Velocity.Y);
}

EMovementMode ATimeThiefNetworkCharacterBase::GetLocalControlMovementMode() const
{
	auto CMC = GetCharacterMovement();
	if (CMC == nullptr)
	{
		return EMovementMode::MOVE_None;
	}
	
	return CMC->MovementMode;
}

FVector ATimeThiefNetworkCharacterBase::GetMoveStep() const
{
	if (NetworkMoveComponent == nullptr)
	{
		return FVector::ZeroVector;
	}
	
	return NetworkMoveComponent->GetMoveStep();
}

void ATimeThiefNetworkCharacterBase::ApplyNetworkMovementState(const FNetworkEntityState& EntityState)
{
	if (NetworkMoveComponent == nullptr)
	{
		return;
	}
	
	NetworkMoveComponent->ApplyNetworkState(EntityState);
}

class UNetworkCombatSyncComponent* ATimeThiefNetworkCharacterBase::GetCombatSyncComponent() const
{
	return GetNetworkCombatSyncComponent();
}

class UTimeThiefPawnCombatComponent* ATimeThiefNetworkCharacterBase::GetCombatComponent() const
{
	return ATimeThiefCharacterBase::GetCombatComponent();
}

uint32 ATimeThiefNetworkCharacterBase::GetCombatEntityId() const
{
	return GetEntityId();
}

bool ATimeThiefNetworkCharacterBase::IsLocalPlayer() const
{
	return NetworkEntityComponent && NetworkEntityComponent->IsLocalControlled();
}

uint32 ATimeThiefNetworkCharacterBase::GetEntityId() const
{
	return NetworkEntityComponent ? NetworkEntityComponent->GetEntityId() : 0;
}
