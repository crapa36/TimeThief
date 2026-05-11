#include "Components/TimeThiefHeroComponent.h"
#include "Character/TimeThiefPawnData.h"
#include "Input/TimeThiefInputComponent.h"
#include "Input/TimeThiefInputConfig.h"
#include "TimeThiefGameplayTags.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Components/Combat/TimeThiefPawnCombatComponent.h"
#include "Components/Combat/TimeThiefThrowableComponent.h"
#include "Components/Wire/TimeThiefWireComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Character/TimeThiefCharacterBase.h"
#include "Character/TimeThiefPlayerCharacter.h"
#include "Character/TimeThiefPlayerController.h"
#include "Network/NetworkGameInstanceSubsystem.h"
#include "Skill/SavePointSkillComponent.h"
#include "InputCoreTypes.h"

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
	Super::SetPawnData(InPawnData);
}

void UTimeThiefHeroComponent::OnRegister()
{
	Super::OnRegister();
	UGameFrameworkComponentManager::AddGameFrameworkComponentReceiver(GetOwner());
}

void UTimeThiefHeroComponent::BeginPlay()
{
	Super::BeginPlay();
	BindOnActorInitStateChanged(NAME_None, FGameplayTag(), false);
	CheckDefaultInitialization();
	RebuildCachedComponents();
}

void UTimeThiefHeroComponent::RebuildCachedComponents()
{
	if (APawn* Pawn = GetPawn<APawn>())
	{
		CachedWireComponent = Pawn->FindComponentByClass<UTimeThiefWireComponent>();
		CachedCombatComponent = Pawn->FindComponentByClass<UTimeThiefPawnCombatComponent>();
		CachedThrowableComponent = Pawn->FindComponentByClass<UTimeThiefThrowableComponent>();

#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Warning, TEXT("[ThrowableDebug][Hero] RebuildCachedComponents Pawn=%s Throwable=%s"),
			*GetNameSafe(Pawn),
			*GetNameSafe(CachedThrowableComponent));
#endif
		return;
	}

	CachedWireComponent = nullptr;
	CachedCombatComponent = nullptr;
	CachedThrowableComponent = nullptr;
}

void UTimeThiefHeroComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UGameFrameworkComponentManager::RemoveGameFrameworkComponentReceiver(GetOwner());
	Super::EndPlay(EndPlayReason);
}

void UTimeThiefHeroComponent::InitializeAbilitySystem()
{
}

const UTimeThiefInputConfig* UTimeThiefHeroComponent::GetInputConfig() const
{
	return PawnData ? PawnData->InputConfig : nullptr;
}

void UTimeThiefHeroComponent::InitializePlayerInput(UInputComponent* PlayerInputComponent)
{
	check(PlayerInputComponent);
	if (!PawnData || bReadyToBindInputs) return;

	APawn* Pawn = GetPawn<APawn>();
	if (!Pawn || !Pawn->IsLocallyControlled()) return;

	UTimeThiefInputComponent* TimeThiefIC = Cast<UTimeThiefInputComponent>(PlayerInputComponent);
	if (!TimeThiefIC) return;

	const UTimeThiefInputConfig* InputConfig = PawnData->InputConfig;
	for (const TObjectPtr<UInputMappingContext>& IMC : PawnData->InputMappingContexts)
	{
		if (IMC) AddInputMappingContext(IMC, 1);
	}

	const FTimeThiefGameplayTags& Tags = FTimeThiefGameplayTags::Get();

	TimeThiefIC->BindNativeAction(InputConfig, Tags.InputTag_Action_Move, ETriggerEvent::Triggered, this, &ThisClass::Input_Move);
	TimeThiefIC->BindNativeAction(InputConfig, Tags.InputTag_Action_Move, ETriggerEvent::Completed, this, &ThisClass::Input_MoveCompleted);
	TimeThiefIC->BindNativeAction(InputConfig, Tags.InputTag_Action_Look, ETriggerEvent::Triggered, this, &ThisClass::Input_Look);
	TimeThiefIC->BindNativeAction(InputConfig, Tags.InputTag_Action_Jump, ETriggerEvent::Started, this, &ThisClass::Input_Jump);
	TimeThiefIC->BindNativeAction(InputConfig, Tags.InputTag_Action_TogglePerspective, ETriggerEvent::Started, this, &ThisClass::Input_TogglePerspective);
	TimeThiefIC->BindNativeAction(InputConfig, Tags.InputTag_Action_ToggleMinimap, ETriggerEvent::Started, this, &ThisClass::Input_ToggleMinimap);
	TimeThiefIC->BindNativeAction(InputConfig, Tags.InputTag_Action_Interact, ETriggerEvent::Started, this, &ThisClass::Input_Interact);
	TimeThiefIC->BindNativeAction(InputConfig, Tags.InputTag_Action_Inventory, ETriggerEvent::Started, this, &ThisClass::Input_ToggleInventory);
	TimeThiefIC->BindNativeAction(InputConfig, Tags.InputTag_Action_WheelMenu, ETriggerEvent::Started, this, &ThisClass::Input_WheelMenu);
	TimeThiefIC->BindNativeAction(InputConfig, Tags.InputTag_Action_WheelMenu, ETriggerEvent::Completed, this, &ThisClass::Input_WheelMenu);
	TimeThiefIC->BindNativeAction(InputConfig, Tags.InputTag_Action_SavePoint, ETriggerEvent::Started, this, &ThisClass::Input_SavePoint);
	
	TArray<uint32> BindHandles;
	TimeThiefIC->BindAbilityActions(InputConfig, this, &ThisClass::Input_AbilityInputTagPressed, &ThisClass::Input_AbilityInputTagReleased, BindHandles);

	if (!InputConfig->FindAbilityInputActionForTag(Tags.InputTag_Action_Throw, false))
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Warning, TEXT("[ThrowableDebug][Hero] No IA mapped for InputTag.Action.Throw. Binding fallback G key."));
#endif
		BindThrowFallbackInput(TimeThiefIC);
	}
	else
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Warning, TEXT("[ThrowableDebug][Hero] InputTag.Action.Throw is mapped in InputConfig=%s."),
			*GetNameSafe(InputConfig));
#endif
	}

	bReadyToBindInputs = true;
	OnReadyToBindInputs.Broadcast(this);
	
	SetReadyToBindInputs();
}

void UTimeThiefHeroComponent::AddInputMappingContext(const UInputMappingContext* MappingContext, int32 Priority)
{
	if (APawn* Pawn = GetPawn<APawn>())
	{
		if (APlayerController* PC = Pawn->GetController<APlayerController>())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
			{
				Subsystem->AddMappingContext(MappingContext, Priority);
			}
		}
	}
}

void UTimeThiefHeroComponent::RemoveInputMappingContext(const UInputMappingContext* MappingContext)
{
	if (APawn* Pawn = GetPawn<APawn>())
	{
		if (APlayerController* PC = Pawn->GetController<APlayerController>())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
			{
				Subsystem->RemoveMappingContext(MappingContext);
			}
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
	Character->AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), MovementVector.Y);
	Character->AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), MovementVector.X);
	if (CachedWireComponent) CachedWireComponent->SetMoveInput(MovementVector);

	Character->GetCharacterMovement()->bUseControllerDesiredRotation = true;
}

void UTimeThiefHeroComponent::Input_MoveCompleted(const FInputActionValue& Value)
{
	if (CachedWireComponent) CachedWireComponent->SetMoveInput(FVector2D::ZeroVector);

	if (ACharacter* Character = GetPawn<ACharacter>())
	{
		Character->GetCharacterMovement()->bUseControllerDesiredRotation = false;
	}
}

void UTimeThiefHeroComponent::Input_Look(const FInputActionValue& Value)
{
	APawn* Pawn = GetPawn<APawn>();
	if (!Pawn) return;
	FVector2D LookAxisVector = Value.Get<FVector2D>();
	if (APlayerController* PC = Pawn->GetController<APlayerController>())
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
	if (ACharacter* Character = GetPawn<ACharacter>()) Character->Jump();
}

void UTimeThiefHeroComponent::Input_TogglePerspective(const FInputActionValue& Value)
{
	if (ATimeThiefCharacterBase* Character = Cast<ATimeThiefCharacterBase>(GetPawn())) Character->TogglePerspective();
}

void UTimeThiefHeroComponent::Input_ToggleMinimap(const FInputActionValue& Value)
{
	if (ATimeThiefPlayerCharacter* Player = Cast<ATimeThiefPlayerCharacter>(GetPawn()))
	{
		if (ATimeThiefPlayerController* PC = Cast<ATimeThiefPlayerController>(Player->GetController())) PC->ToggleWidget(EWidgetType::Minimap);
	}
}

void UTimeThiefHeroComponent::Input_Interact(const FInputActionValue& Value)
{
	if (ATimeThiefPlayerCharacter* Player = Cast<ATimeThiefPlayerCharacter>(GetPawn())) Player->OnInteract();
}

void UTimeThiefHeroComponent::Input_ToggleInventory(const FInputActionValue& Value)
{
	if (ATimeThiefPlayerCharacter* Player = Cast<ATimeThiefPlayerCharacter>(GetPawn()))
	{
		if (ATimeThiefPlayerController* PC = Cast<ATimeThiefPlayerController>(Player->GetController())) PC->ToggleWidget(EWidgetType::Inventory);
	}
}

void UTimeThiefHeroComponent::Input_WheelMenu(const FInputActionValue& Value)
{
	if (auto Player = Cast<ATimeThiefPlayerCharacter>(GetPawn()))
	{
		if (auto PC = Cast<ATimeThiefPlayerController>(Player->GetController()))
		{
			PC->SetVisibilityWidget(EWidgetType::WheelMenu, Value.Get<bool>());
		}
	}
}

void UTimeThiefHeroComponent::Input_SavePoint(const FInputActionValue& Value)
{
	if (auto Player = Cast<ATimeThiefPlayerCharacter>(GetPawn()))
	{
		if (auto SaveSkill = Player->GetSavePointSkillComponent())
		{
			if (SaveSkill->CanActivate())
			{
				if (UNetworkGameInstanceSubsystem* NGIS = UNetworkGameInstanceSubsystem::Get(this))
				{
					NGIS->SendSavePointSet(Player->GetActorLocation());
				}
			}
		}
	}
}

void UTimeThiefHeroComponent::Input_ThrowFallback(const FInputActionValue& Value)
{
#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Warning, TEXT("[ThrowableDebug][Hero] Fallback G input triggered. Throwable=%s"),
		*GetNameSafe(CachedThrowableComponent));
#endif

	if (CachedThrowableComponent)
	{
		CachedThrowableComponent->HandleInputPressed(FTimeThiefGameplayTags::Get().InputTag_Action_Throw);
	}
}

void UTimeThiefHeroComponent::BindThrowFallbackInput(UTimeThiefInputComponent* TimeThiefIC)
{
	if (!TimeThiefIC)
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Warning, TEXT("[ThrowableDebug][Hero] BindThrowFallbackInput failed: InputComponent is null."));
#endif
		return;
	}

	if (!FallbackThrowInputAction)
	{
		FallbackThrowInputAction = NewObject<UInputAction>(this, TEXT("IA_Throw_Fallback"));
		FallbackThrowInputAction->ValueType = EInputActionValueType::Boolean;
	}

	if (!FallbackThrowMappingContext)
	{
		FallbackThrowMappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Throw_Fallback"));
		FallbackThrowMappingContext->MapKey(FallbackThrowInputAction, EKeys::G);
	}

	AddInputMappingContext(FallbackThrowMappingContext, 2);
	TimeThiefIC->BindAction(FallbackThrowInputAction, ETriggerEvent::Started, this, &ThisClass::Input_ThrowFallback);

#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Warning, TEXT("[ThrowableDebug][Hero] Fallback throw input bound. IA=%s IMC=%s Key=G"),
		*GetNameSafe(FallbackThrowInputAction),
		*GetNameSafe(FallbackThrowMappingContext));
#endif
}

void UTimeThiefHeroComponent::Input_AbilityInputTagPressed(FGameplayTag InputTag)
{
#if !UE_BUILD_SHIPPING
	if (InputTag == FTimeThiefGameplayTags::Get().InputTag_Action_Throw)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ThrowableDebug][Hero] Ability input Throw tag pressed. Throwable=%s"),
			*GetNameSafe(CachedThrowableComponent));
	}
#endif

	if (CachedCombatComponent) CachedCombatComponent->HandleInputPressed(InputTag);
	if (CachedWireComponent) CachedWireComponent->HandleInputPressed(InputTag);
	if (CachedThrowableComponent) CachedThrowableComponent->HandleInputPressed(InputTag);
}

void UTimeThiefHeroComponent::Input_AbilityInputTagReleased(FGameplayTag InputTag)
{
	if (CachedCombatComponent) CachedCombatComponent->HandleInputReleased(InputTag);
}

void UTimeThiefHeroComponent::HandleChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState)
{
	Super::HandleChangeInitState(Manager, CurrentState, DesiredState);

	const FTimeThiefGameplayTags& Tags = FTimeThiefGameplayTags::Get();

	if (DesiredState == Tags.InitState_DataInitialized)
	{
		if (APawn* Pawn = GetPawn<APawn>())
		{
			if (UInputComponent* IC = Pawn->InputComponent)
			{
				InitializePlayerInput(IC);
			}
		}
	}
}
