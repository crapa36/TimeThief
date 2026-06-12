#include "UI/GameResultWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Network/NetworkGameInstanceSubsystem.h"

void UGameResultWidget::SetGameResult(int32 InRank, int32 InScore, const FString& InKillerName)
{
	Rank = InRank;
	Score = InScore;
	KillerName = InKillerName;

	RefreshResultTexts();
}

void UGameResultWidget::SetLeavePending(bool bInLeavePending)
{
	bLeavePending = bInLeavePending;
	RefreshLeaveButton();
}

UWidget* UGameResultWidget::GetInitialFocusWidget() const
{
	return LeaveRoom_Button && LeaveRoom_Button->GetIsFocusable() ? LeaveRoom_Button : nullptr;
}

void UGameResultWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (LeaveRoom_Button)
	{
		LeaveRoom_Button->OnClicked.RemoveDynamic(this, &UGameResultWidget::OnLeaveRoomClicked);
		LeaveRoom_Button->OnClicked.AddDynamic(this, &UGameResultWidget::OnLeaveRoomClicked);
	}

	RefreshResultTexts();
	RefreshLeaveButton();
}

void UGameResultWidget::NativeDestruct()
{
	if (LeaveRoom_Button)
	{
		LeaveRoom_Button->OnClicked.RemoveDynamic(this, &UGameResultWidget::OnLeaveRoomClicked);
	}

	Super::NativeDestruct();
}

void UGameResultWidget::RefreshResultTexts()
{
	if (Rank_Text)
	{
		Rank_Text->SetText(FText::Format(NSLOCTEXT("TimeThief", "GameResultRankBadge", "#{0}"), FText::AsNumber(Rank)));
	}

	if (FinalRank_Text)
	{
		FinalRank_Text->SetText(FText::Format(NSLOCTEXT("TimeThief", "GameResultFinalRank", "최종 등수 {0}위"), FText::AsNumber(Rank)));
	}

	if (Score_Text)
	{
		Score_Text->SetText(FText::Format(NSLOCTEXT("TimeThief", "GameResultScore", "최종 스코어 : {0}"), FText::AsNumber(Score)));
	}

	if (ResultMessage_Text)
	{
		if (KillerName.IsEmpty() || KillerName.Equals(TEXT("You are Victorious!")))
		{
			ResultMessage_Text->SetText(NSLOCTEXT("TimeThief", "GameResultVictory", "승리!"));
		}
		else
		{
			ResultMessage_Text->SetText(FText::Format(NSLOCTEXT("TimeThief", "GameResultKilledBy", "{0} 에게 살해 당했습니다."), FText::FromString(KillerName)));
		}
	}
}

void UGameResultWidget::RefreshLeaveButton()
{
	if (LeaveRoom_Button)
	{
		LeaveRoom_Button->SetIsEnabled(!bLeavePending);
	}
}

void UGameResultWidget::OnLeaveRoomClicked()
{
	if (UNetworkGameInstanceSubsystem* NGIS = UNetworkGameInstanceSubsystem::Get(this))
	{
		NGIS->RequestRoomLeave();
	}
}
