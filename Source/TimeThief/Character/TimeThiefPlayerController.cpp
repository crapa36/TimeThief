#include "Character/TimeThiefPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "TimeThief.h"
#include "TimeThiefPlayerCharacter.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Network/TestPlayer/NTCheatManager.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "UI/TimeThiefHUDWidget.h"
#include "UI/Inventory/InventoryWidget.h"

ATimeThiefPlayerController::ATimeThiefPlayerController()
{
	CheatClass = UNTCheatManager::StaticClass();
}

void ATimeThiefPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalPlayerController())
	{
		if (ShouldUseTouchControls())
		{
			MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

			if (MobileControlsWidget)
			{
				MobileControlsWidget->AddToPlayerScreen(0);
			}
			else
			{
				UE_LOG(LogTimeThief, Error, TEXT("Could not spawn mobile controls widget."));
			}
		}

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
				
				if (auto Inventory = Cast<UInventoryWidget>(SubWidgets[static_cast<int>(Pair.Key)]))
				{
					UKismetSystemLibrary::PrintString(this, TEXT("Initializing Inventory Widget"));
					Inventory->Init(Cast<ATimeThiefPlayerCharacter>(GetPawn()));
				}
			}
		}
	}
}

void ATimeThiefPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (IsLocalPlayerController())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<
			UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
}

bool ATimeThiefPlayerController::ShouldUseTouchControls() const
{
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

void ATimeThiefPlayerController::ToggleWidget(EWidgetType WidgetType)
{
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
	if (auto& TargetWidget = SubWidgets[static_cast<int>(WidgetType)])
	{
		if (bVisible != TargetWidget->IsVisible())
		{
			ToggleWidget(WidgetType);
		}
	}
}
