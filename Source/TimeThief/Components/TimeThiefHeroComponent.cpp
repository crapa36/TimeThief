#include "Components/TimeThiefHeroComponent.h"
#include "Character/TimeThiefPawnData.h"
#include "Input/TimeThiefInputComponent.h"
#include "Input/TimeThiefInputConfig.h"
#include "TimeThiefGameplayTags.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Components/Combat/TimeThiefPawnCombatComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputSubsystems.h"

UTimeThiefHeroComponent::UTimeThiefHeroComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer) {
	bWantsInitializeComponent = true;
}

UTimeThiefHeroComponent* UTimeThiefHeroComponent::FindHeroComponent(const AActor* Actor) {
	return (Actor ? Actor->FindComponentByClass<UTimeThiefHeroComponent>() : nullptr);
}

void UTimeThiefHeroComponent::SetPawnData(const UTimeThiefPawnData* InPawnData) {
	check(InPawnData);

	if (PawnData) {
		return;
	}

	PawnData = InPawnData;

	CheckDefaultInitialization();
}

void UTimeThiefHeroComponent::OnRegister() {
	Super::OnRegister();

	const APawn* Pawn = GetPawn<APawn>();
	if (!ensure(Pawn)) {
		return;
	}

	UGameFrameworkComponentManager::AddGameFrameworkComponentReceiver(GetOwner());
}

void UTimeThiefHeroComponent::BeginPlay() {
	Super::BeginPlay();

	BindOnActorInitStateChanged(NAME_None, FGameplayTag(), false);

	CheckDefaultInitialization();
}

void UTimeThiefHeroComponent::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	UGameFrameworkComponentManager::RemoveGameFrameworkComponentReceiver(GetOwner());
	Super::EndPlay(EndPlayReason);
}

void UTimeThiefHeroComponent::InitializePlayerInput(UInputComponent* PlayerInputComponent) {
	check(PlayerInputComponent);

	if (!PawnData) {
		return;
	}

	if (bReadyToBindInputs) {
		return;
	}

	if (UTimeThiefInputComponent* TimeThiefIC = Cast<UTimeThiefInputComponent>(PlayerInputComponent)) {
		if (const UTimeThiefInputConfig* InputConfig = PawnData->InputConfig) {
			const FTimeThiefGameplayTags& GameplayTags = FTimeThiefGameplayTags::Get();

			TimeThiefIC->BindNativeAction(InputConfig, GameplayTags.InputTag_Action_Move, ETriggerEvent::Triggered, this, &UTimeThiefHeroComponent::Input_Move);
			TimeThiefIC->BindNativeAction(InputConfig, GameplayTags.InputTag_Action_Look, ETriggerEvent::Triggered, this, &UTimeThiefHeroComponent::Input_Look);
			TimeThiefIC->BindNativeAction(InputConfig, GameplayTags.InputTag_Action_Jump, ETriggerEvent::Started, this, &UTimeThiefHeroComponent::Input_Jump);

			TArray<uint32> BindHandles;
			TimeThiefIC->BindAbilityActions(InputConfig, this, &UTimeThiefHeroComponent::Input_AbilityInputTagPressed, &UTimeThiefHeroComponent::Input_AbilityInputTagReleased, BindHandles);
		}
	}

	if (!bReadyToBindInputs) {
		bReadyToBindInputs = true;
		OnReadyToBindInputs.Broadcast(this);
	}
}

void UTimeThiefHeroComponent::AddAdditionalInputConfig(const UTimeThiefInputConfig* InputConfig) {
}

void UTimeThiefHeroComponent::Input_Move(const FInputActionValue& Value) {
	ACharacter* Character = GetPawn<ACharacter>();
	if (!Character) {
		return;
	}

	const FVector2D MovementVector = Value.Get<FVector2D>();

	if (AController* Controller = Character->GetController()) {
		const FRotator ControlRotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, ControlRotation.Yaw, 0);

		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		Character->AddMovementInput(ForwardDirection, MovementVector.Y);
		Character->AddMovementInput(RightDirection, MovementVector.X);

		if (!FMath::IsNearlyZero(MovementVector.SizeSquared())) {
			const FRotator CurrentRotation = Character->GetActorRotation();
			const FRotator TargetRotation = FMath::RInterpTo(CurrentRotation, YawRotation, GetWorld()->GetDeltaSeconds(), RotationInterpSpeed);
			Character->SetActorRotation(TargetRotation);
		}
	}
}

void UTimeThiefHeroComponent::Input_Look(const FInputActionValue& Value) {
	APawn* Pawn = GetPawn<APawn>();
	if (!Pawn) {
		return;
	}

	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (APlayerController* PlayerController = Cast<APlayerController>(Pawn->GetController())) {
		PlayerController->AddYawInput(LookAxisVector.X);
		PlayerController->AddPitchInput(LookAxisVector.Y);
	}
}

void UTimeThiefHeroComponent::Input_Jump(const FInputActionValue& Value) {
	ACharacter* Character = GetPawn<ACharacter>();
	if (!Character) {
		return;
	}

	Character->Jump();
}

void UTimeThiefHeroComponent::Input_AbilityInputTagPressed(FGameplayTag InputTag) {
	if (APawn* Pawn = GetPawn<APawn>()) {
		if (UTimeThiefPawnCombatComponent* CombatComp = Pawn->FindComponentByClass<UTimeThiefPawnCombatComponent>()) {
			CombatComp->HandleInputPressed(InputTag);
		}
	}
}

void UTimeThiefHeroComponent::Input_AbilityInputTagReleased(FGameplayTag InputTag) {
	if (APawn* Pawn = GetPawn<APawn>()) {
		if (UTimeThiefPawnCombatComponent* CombatComp = Pawn->FindComponentByClass<UTimeThiefPawnCombatComponent>()) {
			CombatComp->HandleInputReleased(InputTag);
		}
	}
}

