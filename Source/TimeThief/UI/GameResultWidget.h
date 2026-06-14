#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameResultWidget.generated.h"

class UButton;
class UTextBlock;
class UWidget;

UCLASS(Abstract)
class TIMETHIEF_API UGameResultWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "TimeThief|Game Result")
	void SetGameResult(int32 InRank, int32 InScore, const FString& InKillerName);

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Game Result")
	void SetLeavePending(bool bInLeavePending);

	UWidget* GetInitialFocusWidget() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void RefreshResultTexts();
	void RefreshLeaveButton();

	UFUNCTION()
	void OnLeaveRoomClicked();

private:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Rank_Text;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> FinalRank_Text;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Score_Text;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ResultMessage_Text;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> LeaveRoom_Button;

	int32 Rank = 0;
	int32 Score = 0;
	FString KillerName;
	bool bLeavePending = false;
};
