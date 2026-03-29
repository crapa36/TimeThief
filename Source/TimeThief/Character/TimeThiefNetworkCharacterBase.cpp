


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

float ATimeThiefNetworkCharacterBase::GetNetworkPitch() const
{
	return CurrentNetworkPitch;
}

void ATimeThiefNetworkCharacterBase::SetNetworkPitch(float NewPitch)
{
	const float NormalizedPitch = FRotator::NormalizeAxis(NewPitch);
	CurrentNetworkPitch = FMath::Clamp(NormalizedPitch, -89.0f, 89.0f);
}

float ATimeThiefNetworkCharacterBase::GetNetworkSpeed() const
{
	return CurrentNetworkSpeed;
}

void ATimeThiefNetworkCharacterBase::SetNetworkSpeed(float NewSpeed)
{
	CurrentNetworkPitch = NewSpeed;
}

float ATimeThiefNetworkCharacterBase::GetLocalControlPitch() const
{
	if (Controller == nullptr)
	{
		return 0.0f;
	}
	
	float ControlPitch = Controller->GetControlRotation().Pitch;
	ControlPitch = FRotator::NormalizeAxis(ControlPitch);
	return FMath::Clamp(ControlPitch, -89.0f, 89.0f);
}

float ATimeThiefNetworkCharacterBase::GetLocalControlSpeed() const
{
	if (GetCharacterMovement() == nullptr)
	{
		return 0.0f;
	}
	
	return GetCharacterMovement()->Velocity.Size();
}

FVector ATimeThiefNetworkCharacterBase::GetNetworkVelocity() const
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
