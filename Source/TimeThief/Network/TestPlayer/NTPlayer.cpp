


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
	// GetCapsuleComponent()->InitCapsuleSize(50.f, 100.f);
	//
	// bUseControllerRotationPitch = false;
	// bUseControllerRotationYaw = false;
	// bUseControllerRotationRoll = false;
	//
	// GetCharacterMovement()->bOrientRotationToMovement = true;
	// GetCharacterMovement()->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
	//
	// GetCharacterMovement()->JumpZVelocity = 600.f;
	// GetCharacterMovement()->AirControl = 0.2f;
	// GetCharacterMovement()->MaxWalkSpeed = 600.f;
	// GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	// GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	// GetCharacterMovement()->BrakingDecelerationFalling = 1500.f;
	//
	// GetCharacterMovement()->bRunPhysicsWithNoController = true;
	//
	// GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -100.0f));
	// GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	
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

// Called to bind functionality to input
void ANTPlayer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

