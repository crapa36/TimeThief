#include "Character/TimeThiefPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "Character/TimeThiefPlayerCharacter.h"
#include "Network/NetworkGameInstanceSubsystem.h"
#include "Network/TestPlayer/NTCheatManager.h"
#include "UI/MainMenuWidget.h"
#include "UI/TimeThiefHUDWidget.h"
#include "UI/Inventory/InventoryWidget.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Game/TimeThiefGameMode.h"
#include "TimeThiefGameplayTags.h"
#include "Components/TimeThiefPawnExtensionComponent.h"

ATimeThiefPlayerController::ATimeThiefPlayerController()
{
	CheatClass = UNTCheatManager::StaticClass();
}

void ATimeThiefPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalPlayerController())
	{
		if (UNetworkGameInstanceSubsystem* NGIS = UNetworkGameInstanceSubsystem::Get(this))
		{
			NGIS->OnPlayStateChanged.AddUniqueDynamic(this, &ATimeThiefPlayerController::HandleNetworkPlayStateChanged);
		
			if (NGIS->GetPlayState() == ENetworkPlayState::InLobby)
			{
				ShowMainMenu();
			}
		}
	}
}

void ATimeThiefPlayerController::ShowMainMenu()
{
	if (MainMenuWidget || !MainMenuWidgetClass)
	{
		return;
	}

	MainMenuWidget = CreateWidget<UMainMenuWidget>(this, MainMenuWidgetClass);
	if (!MainMenuWidget)
	{
		return;
	}

	MainMenuWidget->AddToViewport(10);

	bShowMouseCursor = true;
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(MainMenuWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
}

void ATimeThiefPlayerController::HideMainMenu()
{
	if (MainMenuWidget)
	{
		MainMenuWidget->RemoveFromParent();
		MainMenuWidget = nullptr;
	}

	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly{});
}

void ATimeThiefPlayerController::HandleNetworkPlayStateChanged(ENetworkPlayState NewState)
{
	if (NewState == ENetworkPlayState::InLobby)
	{
		ShowMainMenu();
	}
	else if (NewState == ENetworkPlayState::MatchingSucc)
	{
		HideMainMenu();
	}
}

void ATimeThiefPlayerController::InitializeUI()
{
	if (bUIInitialized || !IsLocalPlayerController())
	{
		return;
	}

	if (MainHUDWidgetClass)
	{
		MainHUDWidget = CreateWidget<UTimeThiefHUDWidget>(this, MainHUDWidgetClass);
		if (MainHUDWidget)
		{
			MainHUDWidget->AddToViewport();
			if (ATimeThiefPlayerCharacter* PlayerCharacter = Cast<ATimeThiefPlayerCharacter>(GetPawn()))
			{
				MainHUDWidget->InitializeHUD(PlayerCharacter);
			}
		}
	}

	SubWidgets.SetNum(static_cast<int>(EWidgetType::SIZE));
	
	for (auto& Pair : SubWidgetClassMap)
	{
		if (Pair.Value)
		{
			SubWidgets[static_cast<int>(Pair.Key)] = CreateWidget<UUserWidget>(this, Pair.Value);
			SubWidgets[static_cast<int>(Pair.Key)]->AddToViewport(1);
			SubWidgets[static_cast<int>(Pair.Key)]->SetVisibility(ESlateVisibility::Hidden);

			if (Pair.Key == EWidgetType::Inventory)
			{
				if (auto Inventory = Cast<UInventoryWidget>(SubWidgets[static_cast<int>(Pair.Key)]))
				{
					if (ATimeThiefPlayerCharacter* PlayerCharacter = Cast<ATimeThiefPlayerCharacter>(GetPawn()))
					{
						Inventory->Init(PlayerCharacter);
					}
				}
			}
		}
	}

	bUIInitialized = true;
}

void ATimeThiefPlayerController::SetPawn(APawn* InPawn)
{
	Super::SetPawn(InPawn);

	if (IsLocalPlayerController() && InPawn)
	{
		if (UGameFrameworkComponentManager* Manager = UGameFrameworkComponentManager::GetForActor(InPawn))
		{
			Manager->RegisterAndCallForActorInitState(InPawn, UTimeThiefPawnExtensionComponent::NAME_ActorFeatureName, FTimeThiefGameplayTags::Get().InitState_GameplayReady, FActorInitStateChangedDelegate::CreateUObject(this, &ThisClass::OnPawnInitStateChanged), true);
		}
	}

	if (IsLocalPlayerController() && bUIInitialized)
	{
		if (SubWidgets.IsValidIndex(static_cast<int>(EWidgetType::Inventory)))
		{
			if (auto Inventory = Cast<UInventoryWidget>(SubWidgets[static_cast<int>(EWidgetType::Inventory)]))
			{
				if (ATimeThiefPlayerCharacter* PlayerCharacter = Cast<ATimeThiefPlayerCharacter>(InPawn))
				{
					Inventory->Init(PlayerCharacter);
				}
				else
				{
					Inventory->Init(nullptr);
				}
			}
		}

		if (MainHUDWidget)
		{
			if (ATimeThiefPlayerCharacter* PlayerCharacter = Cast<ATimeThiefPlayerCharacter>(InPawn))
			{
				MainHUDWidget->InitializeHUD(PlayerCharacter);
			}
		}
	}
}

void ATimeThiefPlayerController::OnPossess(APawn* InPawn)
{
	if (ATimeThiefPlayerCharacter* PlayerCharacter = Cast<ATimeThiefPlayerCharacter>(InPawn))
	{
		if (ATimeThiefGameMode* GM = GetWorld()->GetAuthGameMode<ATimeThiefGameMode>())
		{
			if (const UTimeThiefPawnData* ModePawnData = GM->GetDefaultPawnData())
			{
				PlayerCharacter->SetPawnData(ModePawnData);
			}
		}
	}

	Super::OnPossess(InPawn);
}

void ATimeThiefPlayerController::OnPawnInitStateChanged(const FActorInitStateChangedParams& Params)
{
	if (Params.FeatureName == UTimeThiefPawnExtensionComponent::NAME_ActorFeatureName)
	{
		if (Params.FeatureState == FTimeThiefGameplayTags::Get().InitState_GameplayReady)
		{
			InitializeUI();
		}
	}
}

void ATimeThiefPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (IsLocalPlayerController())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}
		}
		InitializeUI();
	}
}

void ATimeThiefPlayerController::ToggleWidget(EWidgetType WidgetType)
{
	if (!SubWidgets.IsValidIndex(static_cast<int>(WidgetType)))
	{
		return;
	}

	if (auto& TargetWidget = SubWidgets[static_cast<int>(WidgetType)])
	{
		if (TargetWidget->IsVisible())
		{
			TargetWidget->SetVisibility(ESlateVisibility::Hidden);
		}
		else
		{
			for (auto& Widget : SubWidgets)
			{
				if (Widget == nullptr)
				{
					continue;
				}
				if (Widget != TargetWidget)
				{
					if (Widget->IsVisible())
					{
						Widget->SetVisibility(ESlateVisibility::Hidden);
					}
				}
			}
			TargetWidget->SetVisibility(ESlateVisibility::Visible);
		}
	}
}

void ATimeThiefPlayerController::SetVisibilityWidget(EWidgetType WidgetType, bool bVisible)
{
	if (!SubWidgets.IsValidIndex(static_cast<int>(WidgetType)))
	{
		return;
	}

	if (auto& TargetWidget = SubWidgets[static_cast<int>(WidgetType)])
	{
		if (bVisible != TargetWidget->IsVisible())
		{
			ToggleWidget(WidgetType);
		}
	}
}