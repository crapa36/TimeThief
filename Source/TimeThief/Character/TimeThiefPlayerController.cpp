#include "Character/TimeThiefPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "TimeThief.h"
#include "Character/TimeThiefPlayerCharacter.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Network/TestPlayer/NTCheatManager.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "UI/TimeThiefHUDWidget.h"
#include "UI/Inventory/InventoryWidget.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Game/TimeThiefGameMode.h"

ATimeThiefPlayerController::ATimeThiefPlayerController()
{
	CheatClass = UNTCheatManager::StaticClass();
}

void ATimeThiefPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalPlayerController())
	{
		if (MainHUDWidgetClass)
		{
			MainHUDWidget = CreateWidget<UTimeThiefHUDWidget>(this, MainHUDWidgetClass);
			if (MainHUDWidget)
			{
				MainHUDWidget->AddToViewport();
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
	}
}

void ATimeThiefPlayerController::SetPawn(APawn* InPawn)
{
	Super::SetPawn(InPawn);

	if (IsLocalPlayerController())
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
	}
}

void ATimeThiefPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (ATimeThiefPlayerCharacter* PlayerCharacter = Cast<ATimeThiefPlayerCharacter>(InPawn))
	{
		if (ATimeThiefGameMode* GM = GetWorld()->GetAuthGameMode<ATimeThiefGameMode>())
		{
			PlayerCharacter->SetPawnData(GM->GetDefaultPawnData());
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