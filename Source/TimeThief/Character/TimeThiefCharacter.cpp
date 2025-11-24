// TimeThiefCharacter.cpp

#include "TimeThiefCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"

#include "Input/TimeThiefInputComponent.h"
#include "Input/TimeThiefInputConfig.h"
#include "TimeThiefGameplayTags.h"

ATimeThiefCharacter::ATimeThiefCharacter() {
	
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
}

void ATimeThiefCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) {
	// 커스텀 컴포넌트로 캐스팅하여 사용
	UTimeThiefInputComponent* TimeThiefInputComp = CastChecked<UTimeThiefInputComponent>(PlayerInputComponent);

	if (TimeThiefInputComp && InputConfig) {
		// 태그 매니저 가져오기
		const FTimeThiefGameplayTags& GameplayTags = FTimeThiefGameplayTags::Get();

		// 1. 네이티브 액션 바인딩 (이동, 시점, 점프)
		TimeThiefInputComp->BindNativeAction(InputConfig, GameplayTags.InputTag_Action_Move, ETriggerEvent::Triggered, this, &ThisClass::Input_Move);
		TimeThiefInputComp->BindNativeAction(InputConfig, GameplayTags.InputTag_Action_Look, ETriggerEvent::Triggered, this, &ThisClass::Input_Look);
		TimeThiefInputComp->BindNativeAction(InputConfig, GameplayTags.InputTag_Action_Jump, ETriggerEvent::Started, this, &ACharacter::Jump);
		TimeThiefInputComp->BindNativeAction(InputConfig, GameplayTags.InputTag_Action_Jump, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// 2. 어빌리티 액션 바인딩 (나중에 스킬용)
		TArray<uint32> BindHandles;
		TimeThiefInputComp->BindAbilityActions(InputConfig, this, &ThisClass::Input_AbilityInputTagPressed, &ThisClass::Input_AbilityInputTagReleased, BindHandles);
	}
}

void ATimeThiefCharacter::Input_Move(const FInputActionValue& Value) {
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr) {
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void ATimeThiefCharacter::Input_Look(const FInputActionValue& Value) {
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr) {
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void ATimeThiefCharacter::Input_AbilityInputTagPressed(FGameplayTag InputTag) {
	
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Cyan, FString::Printf(TEXT("Tag Pressed: %s"), *InputTag.ToString()));
}

void ATimeThiefCharacter::Input_AbilityInputTagReleased(FGameplayTag InputTag) {
	
}