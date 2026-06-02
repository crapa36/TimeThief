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

namespace
{
#if TIMETHIEF_WITH_NVIDIA_DLSS
bool SetIntCVarByCode(const TCHAR* Name, int32 Value)
{
	if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name))
	{
		CVar->Set(Value, ECVF_SetByCode);
		return true;
	}

	return false;
}

bool SetFloatCVarByCode(const TCHAR* Name, float Value)
{
	if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name))
	{
		CVar->Set(Value, ECVF_SetByCode);
		return true;
	}

	return false;
}

bool SetDLSSGEnabledByCode(bool bEnabled)
{
	bool bApplied = SetIntCVarByCode(TEXT("r.Streamline.DLSSG.Enable"), bEnabled ? 1 : 0);
	bApplied |= SetIntCVarByCode(TEXT("r.Streamline.DLSSG.FramesToGenerate"), 1);
	return bApplied;
}

bool SetNVIDIAReflexByCode(bool bEnabled)
{
	bool bApplied = SetIntCVarByCode(TEXT("r.Streamline.Reflex.Enable"), bEnabled ? 1 : 0);
	bApplied |= SetIntCVarByCode(TEXT("r.Streamline.Reflex.Mode"), bEnabled ? 1 : 0);
	return bApplied;
}

bool SetDLSSSuperResolutionByCode(bool bEnabled, float ScreenPercentage)
{
	bool bApplied = SetIntCVarByCode(TEXT("r.NGX.Enable"), bEnabled ? 1 : 0);
	bApplied |= SetIntCVarByCode(TEXT("r.NGX.DLSS.DenoiserMode"), 0);
	bApplied |= SetIntCVarByCode(TEXT("r.NGX.DLSS.Enable"), bEnabled ? 1 : 0);

	if (bEnabled)
	{
		bApplied |= SetIntCVarByCode(TEXT("r.TemporalAA.Upscaler"), 1);
		bApplied |= SetIntCVarByCode(TEXT("r.TemporalAA.Upsampling"), 1);
		bApplied |= SetFloatCVarByCode(TEXT("r.ScreenPercentage"), ScreenPercentage);
	}

	return bApplied;
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

	constexpr float DLSSQualityScreenPercentage = 77.0f;
	if (!SetDLSSSuperResolutionByCode(true, DLSSQualityScreenPercentage))
	{
		UE_LOG(LogTimeThief, Log, TEXT("DLSS Super Resolution CVars are not registered; NVIDIA DLSS plugin may not be loaded."));
		return;
	}

	UE_LOG(LogTimeThief, Log, TEXT("DLSS Super Resolution requested."));
#else
	UE_LOG(LogTimeThief, Log, TEXT("NVIDIA DLSS modules are not available; skipping DLSS Super Resolution."));
#endif
}

void ATimeThiefPlayerController::ApplyNVIDIAReflexSetting()
{
#if TIMETHIEF_WITH_NVIDIA_DLSS
	if (!bEnableNVIDIAReflex)
	{
		SetNVIDIAReflexByCode(false);
		return;
	}

	if (!SetNVIDIAReflexByCode(true))
	{
		UE_LOG(LogTimeThief, Log, TEXT("NVIDIA Reflex CVars are not registered; NVIDIA Streamline plugin may not be loaded."));
		return;
	}

	UE_LOG(LogTimeThief, Log, TEXT("NVIDIA Reflex requested."));
#else
	UE_LOG(LogTimeThief, Log, TEXT("NVIDIA DLSS modules are not available; skipping NVIDIA Reflex."));
#endif
}

void ATimeThiefPlayerController::ApplyDLSSFrameGenerationSetting()
{
#if TIMETHIEF_WITH_NVIDIA_DLSS
	if (!bEnableDLSSFrameGeneration)
	{
		SetDLSSGEnabledByCode(false);
		return;
	}

	SetIntCVarByCode(TEXT("r.VSync"), 0);
	if (!SetDLSSGEnabledByCode(true))
	{
		UE_LOG(LogTimeThief, Log, TEXT("DLSS Frame Generation CVars are not registered; NVIDIA Streamline plugin may not be loaded."));
		return;
	}

	UE_LOG(LogTimeThief, Log, TEXT("DLSS Frame Generation requested."));
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

	Super::EndPlay(EndPlayReason);
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
