#include "UI/MainMenuWidget.h"
#include "Components/Button.h"
#include "Network/NetworkGameInstanceSubsystem.h"
#include "Kismet/KismetSystemLibrary.h"

void UMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (MatchAction_Button)
	{
		MatchAction_Button->OnClicked.AddDynamic(this, &UMainMenuWidget::OnMatchActionClicked);
	}

	if (ExitGame_Button)
	{
		ExitGame_Button->OnClicked.AddDynamic(this, &UMainMenuWidget::OnExitGameClicked);
	}

	if (UNetworkGameInstanceSubsystem* NGIS = UNetworkGameInstanceSubsystem::Get(this))
	{
		NGIS->OnPlayStateChanged.AddDynamic(this, &UMainMenuWidget::HandlePlayStateChanged);
		CachedPlayState = NGIS->GetPlayState();
	}

	RefreshMatchButtonVisuals();
}

void UMainMenuWidget::NativeDestruct()
{
	if (UNetworkGameInstanceSubsystem* NGIS = UNetworkGameInstanceSubsystem::Get(this))
	{
		NGIS->OnPlayStateChanged.RemoveDynamic(this, &UMainMenuWidget::HandlePlayStateChanged);
	}

	Super::NativeDestruct();
}

void UMainMenuWidget::OnMatchActionClicked()
{
	UNetworkGameInstanceSubsystem* NGIS = UNetworkGameInstanceSubsystem::Get(this);
	if (!NGIS) return;
	
	if (CachedPlayState == ENetworkPlayState::MatchMaking)
	{
		NGIS->RequestMatchQueueCancel();
	}
	else
	{
		NGIS->RequestMatchQueueEnter();
	}
}

void UMainMenuWidget::OnExitGameClicked()
{
	UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
}

void UMainMenuWidget::HandlePlayStateChanged(ENetworkPlayState NewState)
{
	CachedPlayState = NewState;
	RefreshMatchButtonVisuals();
}

void UMainMenuWidget::RefreshMatchButtonVisuals()
{
	if (!MatchAction_Button) return;

	FButtonStyle Style = MatchAction_Button->GetStyle();
	Style.Normal.DrawAs = ESlateBrushDrawType::Image;
	Style.Hovered.DrawAs = ESlateBrushDrawType::Image;
	Style.Pressed.DrawAs = ESlateBrushDrawType::Image;

	if (CachedPlayState == ENetworkPlayState::MatchMaking)
	{
		Style.Normal.SetResourceObject(Tex_Matching);
		Style.Hovered.SetResourceObject(Tex_MatchCancelHover);
		Style.Pressed.SetResourceObject(Tex_MatchCancelHover);
	}
	else
	{
		Style.Normal.SetResourceObject(Tex_MatchStart);
		Style.Hovered.SetResourceObject(Tex_MatchStartHover);
		Style.Pressed.SetResourceObject(Tex_MatchStartHover);
	}
	
	MatchAction_Button->SetStyle(Style);
}