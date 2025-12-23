#include "Character/TimeThiefPlayerCharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Input/TimeThiefInputConfig.h"
#include "Components/Combat/TimeThiefPlayerCombatComponent.h"
#include "EnhancedInputComponent.h"
#include "TimeThiefGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "GAS/TimeThiefAbilitySystemComponent.h"
#include "CharacterTrajectoryComponent.h"
#include "Input/TimeThiefInputComponent.h"

ATimeThiefPlayerCharacter::ATimeThiefPlayerCharacter() {
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	PlayerCombatComponent = CreateDefaultSubobject<UTimeThiefPlayerCombatComponent>(TEXT("PlayerCombatComponent"));

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	CharacterTrajectoryComponent = CreateDefaultSubobject<UCharacterTrajectoryComponent>(TEXT("CharacterTrajectoryComponent"));
	CharacterTrajectoryComponent->SetAutoActivate(true);
	CharacterTrajectoryComponent->PrimaryComponentTick.bCanEverTick = true;
	CharacterTrajectoryComponent->PrimaryComponentTick.bStartWithTickEnabled = true;
}

void ATimeThiefPlayerCharacter::PossessedBy(AController* NewController) {
	Super::PossessedBy(NewController);
	InitAbilityActorInfo();
}

void ATimeThiefPlayerCharacter::OnRep_PlayerState() {
	Super::OnRep_PlayerState();
	InitAbilityActorInfo();
}

UTimeThiefPawnCombatComponent* ATimeThiefPlayerCharacter::GetPawnCombatComponent() const {
	return PlayerCombatComponent;
}

void ATimeThiefPlayerCharacter::InitAbilityActorInfo() {
	Super::InitAbilityActorInfo();

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent()) {
		ASC->InitAbilityActorInfo(this, this);
	}
}

void ATimeThiefPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) {
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UTimeThiefInputComponent* TimeThiefInputComp = Cast<UTimeThiefInputComponent>(PlayerInputComponent)) {
		if (InputConfig) {
			const FTimeThiefGameplayTags& GameplayTags = FTimeThiefGameplayTags::Get();

			TimeThiefInputComp->BindNativeAction(InputConfig, GameplayTags.InputTag_Action_Move, ETriggerEvent::Triggered, this, &ThisClass::Input_Move);
			TimeThiefInputComp->BindNativeAction(InputConfig, GameplayTags.InputTag_Action_Look, ETriggerEvent::Triggered, this, &ThisClass::Input_Look);

			TArray<uint32> BindHandles;
			TimeThiefInputComp->BindAbilityActions(InputConfig, this, &ThisClass::Input_AbilityInputTagPressed, &ThisClass::Input_AbilityInputTagReleased, BindHandles);
		}
	}
}

void ATimeThiefPlayerCharacter::Input_Move(const FInputActionValue& Value) {
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr) {
		const FRotator ControlRotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, ControlRotation.Yaw, 0);

		FRotator CurrentRotation = GetActorRotation();
		FRotator TargetRotation = YawRotation;

		FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, GetWorld()->GetDeltaSeconds(), 15.0f);
		SetActorRotation(NewRotation);

		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void ATimeThiefPlayerCharacter::Input_Look(const FInputActionValue& Value) {
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr) {
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void ATimeThiefPlayerCharacter::Input_AbilityInputTagPressed(FGameplayTag InputTag) {
	if (GetAbilitySystemComponent()) {
		if (UTimeThiefAbilitySystemComponent* TimeThiefASC = Cast<UTimeThiefAbilitySystemComponent>(GetAbilitySystemComponent())) {
			TimeThiefASC->AbilityInputTagPressed(InputTag);
		}
	}
}

void ATimeThiefPlayerCharacter::Input_AbilityInputTagReleased(FGameplayTag InputTag) {
	if (GetAbilitySystemComponent()) {
		if (UTimeThiefAbilitySystemComponent* TimeThiefASC = Cast<UTimeThiefAbilitySystemComponent>(GetAbilitySystemComponent())) {
			TimeThiefASC->AbilityInputTagReleased(InputTag);
		}
	}
}