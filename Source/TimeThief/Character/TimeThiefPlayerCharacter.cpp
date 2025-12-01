#include "Character/TimeThiefPlayerCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputSubsystems.h"
#include "Input/TimeThiefInputComponent.h"
#include "GAS/TimeThiefAbilitySystemComponent.h"
#include "Components/Combat/TimeThiefHeroCombatComponent.h"
#include "TimeThiefGameplayTags.h"

ATimeThiefPlayerCharacter::ATimeThiefPlayerCharacter() {
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	HeroCombatComponent = CreateDefaultSubobject<UTimeThiefHeroCombatComponent>(TEXT("HeroCombatComponent"));
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
	return HeroCombatComponent;
}

void ATimeThiefPlayerCharacter::InitAbilityActorInfo() {
	if (AbilitySystemComponent) {
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
		Super::InitAbilityActorInfo();
	}
}

void ATimeThiefPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) {
	UTimeThiefInputComponent* TimeThiefInputComp = CastChecked<UTimeThiefInputComponent>(PlayerInputComponent);
	if (TimeThiefInputComp && InputConfig) {
		const FTimeThiefGameplayTags& GameplayTags = FTimeThiefGameplayTags::Get();

		TimeThiefInputComp->BindNativeAction(InputConfig, GameplayTags.InputTag_Action_Move, ETriggerEvent::Triggered, this, &ThisClass::Input_Move);
		TimeThiefInputComp->BindNativeAction(InputConfig, GameplayTags.InputTag_Action_Look, ETriggerEvent::Triggered, this, &ThisClass::Input_Look);
		TimeThiefInputComp->BindNativeAction(InputConfig, GameplayTags.InputTag_Action_Jump, ETriggerEvent::Started, this, &ACharacter::Jump);
		TimeThiefInputComp->BindNativeAction(InputConfig, GameplayTags.InputTag_Action_Jump, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		TArray<uint32> BindHandles;
		TimeThiefInputComp->BindAbilityActions(InputConfig, this, &ThisClass::Input_AbilityInputTagPressed, &ThisClass::Input_AbilityInputTagReleased, BindHandles);
	}
}

void ATimeThiefPlayerCharacter::Input_Move(const FInputActionValue& Value) {
	FVector2D MovementVector = Value.Get<FVector2D>();
	if (Controller) {
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
		AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), MovementVector.Y);
		AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), MovementVector.X);
	}
}

void ATimeThiefPlayerCharacter::Input_Look(const FInputActionValue& Value) {
	FVector2D LookAxisVector = Value.Get<FVector2D>();
	if (Controller) {
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void ATimeThiefPlayerCharacter::Input_AbilityInputTagPressed(FGameplayTag InputTag) {
	if (AbilitySystemComponent) AbilitySystemComponent->AbilityInputTagPressed(InputTag);
}

void ATimeThiefPlayerCharacter::Input_AbilityInputTagReleased(FGameplayTag InputTag) {
	if (AbilitySystemComponent) AbilitySystemComponent->AbilityInputTagReleased(InputTag);
}