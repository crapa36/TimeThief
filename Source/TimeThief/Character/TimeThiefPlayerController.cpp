#include "Character/TimeThiefPlayerController.h"
#include "TimeThief.h"
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
#include "HAL/IConsoleManager.h"
#include "UI/GameResultWidget.h"

#ifndef TIMETHIEF_WITH_NVIDIA_DLSS
#define TIMETHIEF_WITH_NVIDIA_DLSS 0
#endif

#if TIMETHIEF_WITH_NVIDIA_DLSS
#include "DLSSLibrary.h"
#include "StreamlineLibraryDLSSG.h"
#include "StreamlineLibraryReflex.h"
#endif

namespace
{
void SetIntCVarByCode(const TCHAR* Name, int32 Value)
{
	if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name))
	{
		CVar->Set(Value, ECVF_SetByCode);
	}
}

void SetFloatCVarByCode(const TCHAR* Name, float Value)
{
	if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name))
	{
		CVar->Set(Value, ECVF_SetByCode);
	}
}

#if TIMETHIEF_WITH_NVIDIA_DLSS
int32 GetDLSSGEnableCVarValue(EStreamlineDLSSGMode Mode)
{
	switch (Mode)
	{
	case EStreamlineDLSSGMode::Off:
		return 0;
	case EStreamlineDLSSGMode::Auto:
		return 2;
	case EStreamlineDLSSGMode::OnDynamic:
		return 3;
	default:
		return 1;
	}
}

int32 GetDLSSGFramesToGenerateCVarValue(EStreamlineDLSSGMode Mode)
{
	switch (Mode)
	{
	case EStreamlineDLSSGMode::On3X:
		return 2;
	case EStreamlineDLSSGMode::On4X:
		return 3;
	case EStreamlineDLSSGMode::On5X:
		return 4;
	case EStreamlineDLSSGMode::On6X:
		return 5;
	default:
		return 1;
	}
}

void SetDLSSGModeByCode(EStreamlineDLSSGMode Mode)
{
	SetIntCVarByCode(TEXT("r.Streamline.DLSSG.Enable"), GetDLSSGEnableCVarValue(Mode));
	SetIntCVarByCode(TEXT("r.Streamline.DLSSG.FramesToGenerate"), GetDLSSGFramesToGenerateCVarValue(Mode));
}

void SetDLSSSuperResolutionByCode(bool bEnabled, float ScreenPercentage)
{
	SetIntCVarByCode(TEXT("r.NGX.DLSS.DenoiserMode"), 0);
	SetIntCVarByCode(TEXT("r.NGX.DLSS.Enable"), bEnabled ? 1 : 0);

	if (bEnabled)
	{
		SetIntCVarByCode(TEXT("r.TemporalAA.Upscaler"), 1);
		SetIntCVarByCode(TEXT("r.TemporalAA.Upsampling"), 1);
		SetFloatCVarByCode(TEXT("r.ScreenPercentage"), ScreenPercentage);
	}
}

UDLSSMode GetPreferredDLSSSuperResolutionMode()
{
	const UDLSSMode PreferredModes[] =
	{
		UDLSSMode::Quality,
		UDLSSMode::Balanced,
		UDLSSMode::Performance,
		UDLSSMode::UltraPerformance
	};

	for (const UDLSSMode Mode : PreferredModes)
	{
		if (UDLSSLibrary::IsDLSSModeSupported(Mode))
		{
			return Mode;
		}
	}

	return UDLSSMode::Off;
}
#endif
}

ATimeThiefPlayerController::ATimeThiefPlayerController()
{
	CheatClass = UNTCheatManager::StaticClass();
}

void ATimeThiefPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalPlayerController())
	{
		ApplyDLSSSuperResolutionSetting();
		ApplyNVIDIAReflexSetting();
		ApplyDLSSFrameGenerationSetting();

		if (UNetworkGameInstanceSubsystem* NGIS = UNetworkGameInstanceSubsystem::Get(this))
		{
			NGIS->OnPlayStateChanged.AddUniqueDynamic(this, &ATimeThiefPlayerController::HandleNetworkPlayStateChanged);
			NGIS->OnPlayerGameResult.AddUniqueDynamic(this, &ATimeThiefPlayerController::HandlePlayerGameResult);
		
			if (NGIS->GetPlayState() == ENetworkPlayState::InLobby)
			{
				ShowMainMenu();
			}
		}
	}
}

void ATimeThiefPlayerController::ApplyDLSSSuperResolutionSetting()
{
#if TIMETHIEF_WITH_NVIDIA_DLSS
	if (!bEnableDLSSSuperResolution)
	{
		SetDLSSSuperResolutionByCode(false, 100.0f);
		return;
	}

	if (!UDLSSLibrary::IsDLSSSupported())
	{
		UE_LOG(LogTimeThief, Log, TEXT("DLSS Super Resolution is not supported on this runtime."));
		return;
	}

	const UDLSSMode DLSSMode = GetPreferredDLSSSuperResolutionMode();
	if (DLSSMode == UDLSSMode::Off)
	{
		UE_LOG(LogTimeThief, Log, TEXT("DLSS Super Resolution is supported, but no quality mode is available."));
		return;
	}

	bool bIsSupported = false;
	bool bIsFixedScreenPercentage = false;
	float OptimalScreenPercentage = 100.0f;
	float MinScreenPercentage = 100.0f;
	float MaxScreenPercentage = 100.0f;
	float OptimalSharpness = 0.0f;
	UDLSSLibrary::GetDLSSModeInformation(
		DLSSMode,
		FVector2D::ZeroVector,
		bIsSupported,
		OptimalScreenPercentage,
		bIsFixedScreenPercentage,
		MinScreenPercentage,
		MaxScreenPercentage,
		OptimalSharpness);

	if (!bIsSupported)
	{
		UE_LOG(LogTimeThief, Log, TEXT("Selected DLSS Super Resolution mode is not supported."));
		return;
	}

	SetDLSSSuperResolutionByCode(true, OptimalScreenPercentage);
	UE_LOG(LogTimeThief, Log, TEXT("DLSS Super Resolution enabled."));
#else
	UE_LOG(LogTimeThief, Log, TEXT("NVIDIA DLSS modules are not available; skipping DLSS Super Resolution."));
#endif
}

void ATimeThiefPlayerController::ApplyNVIDIAReflexSetting()
{
#if TIMETHIEF_WITH_NVIDIA_DLSS
	if (!bEnableNVIDIAReflex)
	{
		UStreamlineLibraryReflex::SetReflexMode(EStreamlineReflexMode::Off);
		return;
	}

	if (!UStreamlineLibraryReflex::IsReflexSupported())
	{
		UE_LOG(LogTimeThief, Log, TEXT("NVIDIA Reflex is not supported on this runtime."));
		return;
	}

	const EStreamlineReflexMode ReflexMode = UStreamlineLibraryReflex::GetDefaultReflexMode();
	if (ReflexMode == EStreamlineReflexMode::Off)
	{
		UE_LOG(LogTimeThief, Log, TEXT("NVIDIA Reflex is supported, but no default mode is available."));
		return;
	}

	UStreamlineLibraryReflex::SetReflexMode(ReflexMode);
	UE_LOG(LogTimeThief, Log, TEXT("NVIDIA Reflex enabled."));
#else
	UE_LOG(LogTimeThief, Log, TEXT("NVIDIA DLSS modules are not available; skipping NVIDIA Reflex."));
#endif
}

void ATimeThiefPlayerController::ApplyDLSSFrameGenerationSetting()
{
#if TIMETHIEF_WITH_NVIDIA_DLSS
	if (!bEnableDLSSFrameGeneration)
	{
		SetDLSSGModeByCode(EStreamlineDLSSGMode::Off);
		return;
	}

	if (!UStreamlineLibraryDLSSG::IsDLSSGSupported())
	{
		UE_LOG(LogTimeThief, Log, TEXT("DLSS Frame Generation is not supported on this runtime."));
		return;
	}

	const EStreamlineDLSSGMode DefaultMode = UStreamlineLibraryDLSSG::GetDefaultDLSSGMode();
	if (DefaultMode == EStreamlineDLSSGMode::Off)
	{
		UE_LOG(LogTimeThief, Log, TEXT("DLSS Frame Generation is supported, but no default mode is available."));
		return;
	}

	SetIntCVarByCode(TEXT("r.VSync"), 0);
	SetDLSSGModeByCode(DefaultMode);
	UE_LOG(LogTimeThief, Log, TEXT("DLSS Frame Generation enabled."));
#else
	UE_LOG(LogTimeThief, Log, TEXT("NVIDIA DLSS modules are not available; skipping DLSS Frame Generation."));
#endif
}

void ATimeThiefPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UNetworkGameInstanceSubsystem* NGIS = UNetworkGameInstanceSubsystem::Get(this))
	{
		NGIS->OnPlayStateChanged.RemoveDynamic(this, &ATimeThiefPlayerController::HandleNetworkPlayStateChanged);
		NGIS->OnPlayerGameResult.RemoveDynamic(this, &ATimeThiefPlayerController::HandlePlayerGameResult);
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

void ATimeThiefPlayerController::ShowGameResult(int32 Rank, int32 Score, const FString& KillerName)
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	if (GameResultWidget == nullptr)
	{
		if (!GameResultWidgetClass)
		{
			return;
		}

		GameResultWidget = CreateWidget<UGameResultWidget>(this, GameResultWidgetClass);
		if (GameResultWidget == nullptr)
		{
			return;
		}

		GameResultWidget->AddToViewport(50);
	}

	GameResultWidget->SetGameResult(Rank, Score, KillerName);
	GameResultWidget->SetLeavePending(false);
	GameResultWidget->SetVisibility(ESlateVisibility::Visible);

	bShowMouseCursor = true;
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(GameResultWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
}

void ATimeThiefPlayerController::HideGameResult()
{
	if (GameResultWidget)
	{
		GameResultWidget->RemoveFromParent();
		GameResultWidget = nullptr;
	}
}

void ATimeThiefPlayerController::HandleNetworkPlayStateChanged(ENetworkPlayState NewState)
{
	if (NewState == ENetworkPlayState::InLobby)
	{
		HideGameResult();
		ShowMainMenu();
	}
	else if (NewState == ENetworkPlayState::MatchingSucc || NewState == ENetworkPlayState::InRoom)
	{
		HideMainMenu();
	}

	if (GameResultWidget)
	{
		GameResultWidget->SetLeavePending(NewState == ENetworkPlayState::LeavingRoom);
	}
}

void ATimeThiefPlayerController::HandlePlayerGameResult(int32 Rank, int32 Score, FString KillerName)
{
	ShowGameResult(Rank, Score, KillerName);
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
