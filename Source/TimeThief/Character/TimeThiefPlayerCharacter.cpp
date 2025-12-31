#include "Character/TimeThiefPlayerCharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/TimeThiefHeroComponent.h"
#include "Components/Combat/TimeThiefPlayerCombatComponent.h"
#include "Character/TimeThiefPawnData.h"
#include "CharacterTrajectoryComponent.h"
#include "Net/UnrealNetwork.h"

ATimeThiefPlayerCharacter::ATimeThiefPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer) {
	
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	HeroComponent = CreateDefaultSubobject<UTimeThiefHeroComponent>(TEXT("HeroComponent"));
	PlayerCombatComponent = CreateDefaultSubobject<UTimeThiefPlayerCombatComponent>(TEXT("PlayerCombatComponent"));

	CharacterTrajectoryComponent = CreateDefaultSubobject<UCharacterTrajectoryComponent>(TEXT("CharacterTrajectoryComponent"));
	CharacterTrajectoryComponent->SetAutoActivate(true);
	CharacterTrajectoryComponent->PrimaryComponentTick.bCanEverTick = true;
	CharacterTrajectoryComponent->PrimaryComponentTick.bStartWithTickEnabled = true;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
}

void ATimeThiefPlayerCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const {
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ATimeThiefPlayerCharacter, PawnData);
}

void ATimeThiefPlayerCharacter::SetPawnData(const UTimeThiefPawnData* InPawnData) {
	check(InPawnData);

	if (PawnData) {
		return;
	}

	PawnData = InPawnData;
	OnPawnDataSet();
}

void ATimeThiefPlayerCharacter::OnPawnDataSet() {
	if (HeroComponent && PawnData) {
		HeroComponent->SetPawnData(PawnData);
		
		if (InputComponent) {
			HeroComponent->InitializePlayerInput(InputComponent);
		}
	}
}

void ATimeThiefPlayerCharacter::PossessedBy(AController* NewController) {
	Super::PossessedBy(NewController);
}

void ATimeThiefPlayerCharacter::UnPossessed() {
	Super::UnPossessed();
}

void ATimeThiefPlayerCharacter::OnRep_PlayerState() {
	Super::OnRep_PlayerState();
}

void ATimeThiefPlayerCharacter::OnRep_PawnData() {
	OnPawnDataSet();
}

UTimeThiefPawnCombatComponent* ATimeThiefPlayerCharacter::GetPawnCombatComponent() const {
	return PlayerCombatComponent;
}

void ATimeThiefPlayerCharacter::BeginPlay() {
	Super::BeginPlay();
}

void ATimeThiefPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) {
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (HeroComponent) {
		HeroComponent->InitializePlayerInput(PlayerInputComponent);
	}
}