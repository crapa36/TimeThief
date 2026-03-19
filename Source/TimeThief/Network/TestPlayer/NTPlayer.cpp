


#include "NTPlayer.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Network/NetworkEntityComponent.h"
#include "Network/NetworkMoveComponent.h"
#include "Network/State/NetworkControlType.h"
#include "Network/State/NetworkEntityState.h"


// Sets default values
ANTPlayer::ANTPlayer()
{
	GetCapsuleComponent()->InitCapsuleSize(50.f, 100.f);
	
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
	
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.2f;
	GetCharacterMovement()->MaxWalkSpeed = 600.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.f;
	
	GetCharacterMovement()->bRunPhysicsWithNoController = true;
	
	GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -100.0f));
	GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	
	NetworkEntityComponent = CreateDefaultSubobject<UNetworkEntityComponent>(TEXT("NetworkEntityComponent"));
	NetworkMoveComponent = CreateDefaultSubobject<UNetworkMoveComponent>(TEXT("NetworkMoveComponent"));
	
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

ANTPlayer::~ANTPlayer()
{
}

// Called when the game starts or when spawned
void ANTPlayer::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ANTPlayer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
}

FVector ANTPlayer::GetNetworkLocation() const
{
	return GetActorLocation();
}

void ANTPlayer::SetNetworkLocation(const FVector& NewLocation)
{
	SetActorLocation(NewLocation);
}

float ANTPlayer::GetNetworkYaw() const
{
	return GetActorRotation().Yaw;
}

void ANTPlayer::SetNetworkYaw(float NewYaw)
{
	FRotator NewRotation = GetActorRotation();
	NewRotation.Yaw = FRotator::NormalizeAxis(NewYaw);
	SetActorRotation(NewRotation);
}

float ANTPlayer::GetNetworkPitch() const
{
	return CurrentNetworkPitch;
}

void ANTPlayer::SetNetworkPitch(float NewPitch)
{
	const float NormalizedPitch = FRotator::NormalizeAxis(CurrentNetworkPitch);
	CurrentNetworkPitch = FMath::Clamp(NormalizedPitch, -89.0f, 89.0f);
}

float ANTPlayer::GetLocalControlPitch() const
{
	if (Controller == nullptr)
	{
		return 0.0f;
	}
	
	float ControlPitch = Controller->GetControlRotation().Pitch;
	ControlPitch = FRotator::NormalizeAxis(ControlPitch);
	return FMath::Clamp(ControlPitch, -89.0f, 89.0f);
}

void ANTPlayer::ApplyNetworkMovementState(const FNetworkEntityState& EntityState)
{
	if (NetworkMoveComponent == nullptr)
	{
		return;
	}
	
	NetworkMoveComponent->ApplyNetworkState(EntityState);
}

UNetworkEntityComponent* ANTPlayer::GetNetworkEntityComponent() const
{
	return NetworkEntityComponent;
}

bool ANTPlayer::IsLocalPlayer() const
{	
	return NetworkEntityComponent && NetworkEntityComponent->IsLocalControlled();
}

uint32 ANTPlayer::GetEntityId() const
{
	return NetworkEntityComponent ? NetworkEntityComponent->GetEntityId() : 0;
}

// Called to bind functionality to input
void ANTPlayer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

