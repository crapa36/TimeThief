#include "Character/TimeThiefPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "TimeThief.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Network/TestPlayer/NTCheatManager.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "UI/TimeThiefHUDWidget.h"
#include "UI/Minimap/MinimapWidget.h"
#include "UI/Store/StoreWidget.h"

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
		
		if (StoreWidgetClass)
		{
			StoreWidget = CreateWidget<UStoreWidget>(this, StoreWidgetClass);
		}
		
		if (MinimapWidgetClass)
		{
			MinimapWidget = CreateWidget<UMinimapWidget>(this, MinimapWidgetClass);
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

void ATimeThiefPlayerController::ToggleMinimap()
{
	IsShowingMinimap = !IsShowingMinimap;
	
	if (IsShowingMinimap)
	{
		MinimapWidget->AddToViewport();
	}
	else
	{
		MinimapWidget->RemoveFromParent();
	}
}

void ATimeThiefPlayerController::SetStoreVisibility(bool bVisible)
{
	if (bVisible)
	{
		if (!StoreWidget->IsInViewport())
		{
			SetIgnoreLookInput(true);
			
			FInputModeGameAndUI InputMode;
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			SetInputMode(InputMode);
			bShowMouseCursor = true;
			
			int32 ViewportSizeX, ViewportSizeY;
			GetViewportSize(ViewportSizeX, ViewportSizeY);
			SetMouseLocation(ViewportSizeX / 2, ViewportSizeY / 2);
			
			StoreWidget->AddToViewport();
		}
	}
	else
	{
		if (StoreWidget->IsInViewport())
		{
			StoreWidget->RemoveFromParent();
			
			SetIgnoreLookInput(false);
			SetInputMode(FInputModeGameOnly{});
			bShowMouseCursor = false;
		}
	}
}
