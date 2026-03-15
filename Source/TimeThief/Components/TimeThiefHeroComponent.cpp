#include "Components/TimeThiefHeroComponent.h"
#include "Character/TimeThiefPawnData.h"
#include "Input/TimeThiefInputComponent.h"
#include "Input/TimeThiefInputConfig.h"
#include "TimeThiefGameplayTags.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Components/Combat/TimeThiefPawnCombatComponent.h"
#include "Components/Wire/TimeThiefWireComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Character/TimeThiefCharacterBase.h"
#include "Character/TimeThiefPlayerCharacter.h"
#include "Character/TimeThiefPlayerController.h"
#include "Weapon/TimeThiefWeaponBase.h"

UTimeThiefHeroComponent::UTimeThiefHeroComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWantsInitializeComponent = true;
}

UTimeThiefHeroComponent* UTimeThiefHeroComponent::FindHeroComponent(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UTimeThiefHeroComponent>() : nullptr;
}

void UTimeThiefHeroComponent::SetPawnData(const UTimeThiefPawnData* InPawnData)
{
	check(InPawnData);
	if (PawnData) return;

	PawnData = InPawnData;
	bPawnDataSet = true;
	CheckDefaultInitialization();
}

void UTimeThiefHeroComponent::OnRegister()
{
	Super::OnRegister();

	if (!ensure(GetPawn<APawn>())) return;

	UGameFrameworkComponentManager::AddGameFrameworkComponentReceiver(GetOwner());
}

void UTimeThiefHeroComponent::BeginPlay()
{
	Super::BeginPlay();
	BindOnActorInitStateChanged(NAME_None, FGameplayTag(), false);
	CheckDefaultInitialization();

	if (APawn* Pawn = GetPawn<APawn>())
	{
		CachedWireComponent = Pawn->FindComponentByClass<UTimeThiefWireComponent>();
		CachedCombatComponent = Pawn->FindComponentByClass<UTimeThiefPawnCombatComponent>();
	}
}

void UTimeThiefHeroComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UGameFrameworkComponentManager::RemoveGameFrameworkComponentReceiver(GetOwner());
	Super::EndPlay(EndPlayReason);
}

void UTimeThiefHeroComponent::InitializePlayerInput(UInputComponent* PlayerInputComponent)
{
	check(PlayerInputComponent);
	if (!PawnData || bReadyToBindInputs) return;

	UTimeThiefInputComponent* TimeThiefIC = Cast<UTimeThiefInputComponent>(PlayerInputComponent);
	if (!TimeThiefIC) return;

	const UTimeThiefInputConfig* InputConfig = PawnData->InputConfig;
	if (!InputConfig) return;

	for (const TObjectPtr<UInputMappingContext>& IMC : PawnData->InputMappingContexts)
	{
		if (IMC)
		{
			AddInputMappingContext(IMC, 1);
		}
	}

	const FTimeThiefGameplayTags& GameplayTags = FTimeThiefGameplayTags::Get();

	TimeThiefIC->BindNativeAction(InputConfig, GameplayTags.InputTag_Action_Move, ETriggerEvent::Triggered, this, &ThisClass::Input_Move);
	TimeThiefIC->BindNativeAction(InputConfig, GameplayTags.InputTag_Action_Move, ETriggerEvent::Completed, this, &ThisClass::Input_MoveCompleted);
	TimeThiefIC->BindNativeAction(InputConfig, GameplayTags.InputTag_Action_Look, ETriggerEvent::Triggered, this, &ThisClass::Input_Look);
	TimeThiefIC->BindNativeAction(InputConfig, GameplayTags.InputTag_Action_Jump, ETriggerEvent::Started, this, &ThisClass::Input_Jump);
	TimeThiefIC->BindNativeAction(InputConfig, GameplayTags.InputTag_Action_TogglePerspective, ETriggerEvent::Started, this, &ThisClass::Input_TogglePerspective);
	TimeThiefIC->BindNativeAction(InputConfig, GameplayTags.InputTag_Action_ToggleMinimap, ETriggerEvent::Started, this, &ThisClass::Input_ToggleMinimap);
	
	TArray<uint32> BindHandles;
	TimeThiefIC->BindAbilityActions(InputConfig, this, &ThisClass::Input_AbilityInputTagPressed, &ThisClass::Input_AbilityInputTagReleased, BindHandles);

	bReadyToBindInputs = true;
	bInputsReady = true;
	OnReadyToBindInputs.Broadcast(this);
	CheckDefaultInitialization();
}

void UTimeThiefHeroComponent::AddInputMappingContext(const UInputMappingContext* MappingContext, int32 Priority)
{
	if (!MappingContext) return;

	APawn* Pawn = GetPawn<APawn>();
	if (!Pawn) return;

	if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(MappingContext, Priority);
		}
	}
}

void UTimeThiefHeroComponent::RemoveInputMappingContext(const UInputMappingContext* MappingContext)
{
	if (!MappingContext) return;

	APawn* Pawn = GetPawn<APawn>();
	if (!Pawn) return;

	if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->RemoveMappingContext(MappingContext);
		}
	}
}

void UTimeThiefHeroComponent::Input_Move(const FInputActionValue& Value)
{
	ACharacter* Character = GetPawn<ACharacter>();
	if (!Character) return;

	const FVector2D MovementVector = Value.Get<FVector2D>();

	AController* Controller = Character->GetController();
	if (!Controller) return;

	const FRotator ControlRotation = Controller->GetControlRotation();
	const FRotator YawRotation(0, ControlRotation.Yaw, 0);

	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	Character->AddMovementInput(ForwardDirection, MovementVector.Y);
	Character->AddMovementInput(RightDirection, MovementVector.X);


	if (CachedWireComponent)
	{
		CachedWireComponent->SetMoveInput(MovementVector);
	}
}

void UTimeThiefHeroComponent::Input_MoveCompleted(const FInputActionValue& Value)
{
	if (CachedWireComponent)
	{
		CachedWireComponent->SetMoveInput(FVector2D::ZeroVector);
	}
}

void UTimeThiefHeroComponent::Input_Look(const FInputActionValue& Value)
{
	APawn* Pawn = GetPawn<APawn>();
	if (!Pawn) return;

	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
	{
		PC->AddYawInput(LookAxisVector.X);
		PC->AddPitchInput(LookAxisVector.Y);
	}
}

void UTimeThiefHeroComponent::Input_Jump(const FInputActionValue& Value)
{
	if (CachedWireComponent && CachedWireComponent->IsWireAttached())
	{
		CachedWireComponent->Jump();
		return;
	}

	if (ACharacter* Character = GetPawn<ACharacter>())
	{
		Character->Jump();
	}
}

void UTimeThiefHeroComponent::Input_TogglePerspective(const FInputActionValue& Value)
{
	if (ATimeThiefCharacterBase* Character = Cast<ATimeThiefCharacterBase>(GetPawn()))
	{
		Character->TogglePerspective();
	}
}

void UTimeThiefHeroComponent::Input_ToggleMinimap(const FInputActionValue& Value)
{
	if (ATimeThiefPlayerCharacter* Player = Cast<ATimeThiefPlayerCharacter>(GetPawn()))
	{
		if (ATimeThiefPlayerController* PC = Cast<ATimeThiefPlayerController> (Player->GetController()))
		{
			PC->ToggleMinimap();
		}
	}
}

void UTimeThiefHeroComponent::Input_AbilityInputTagPressed(FGameplayTag InputTag)
{
	if (CachedCombatComponent)
	{
		CachedCombatComponent->HandleInputPressed(InputTag);
	}
	
	if (CachedWireComponent)
	{
		CachedWireComponent->HandleInputPressed(InputTag);
	}
}

void UTimeThiefHeroComponent::Input_AbilityInputTagReleased(FGameplayTag InputTag)
{
	if (CachedCombatComponent)
	{
		CachedCombatComponent->HandleInputReleased(InputTag);
	}
}
