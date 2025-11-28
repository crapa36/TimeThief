#include "Character/TimeThiefPlayerCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h" 
#include "GameFramework/Controller.h"
#include "EnhancedInputSubsystems.h"
#include "Input/TimeThiefInputComponent.h"
#include "GAS/TimeThiefAbilitySystemComponent.h"
#include "TimeThiefGameplayTags.h"

ATimeThiefPlayerCharacter::ATimeThiefPlayerCharacter() {
	// 카메라 설정
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// 플레이어는 컨트롤러 회전을 따르지 않음 (카메라가 따름)
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
}

void ATimeThiefPlayerCharacter::PossessedBy(AController* NewController) {
	Super::PossessedBy(NewController);
	InitAbilityActorInfo();
}

void ATimeThiefPlayerCharacter::OnRep_PlayerState() {
	Super::OnRep_PlayerState();
	InitAbilityActorInfo();
}

void ATimeThiefPlayerCharacter::InitAbilityActorInfo() {
	if (AbilitySystemComponent) {
		// Owner와 Avatar가 모두 Character인 경우
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
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