#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Network/State/NetworkPlayState.h"
#include "MainMenuWidget.generated.h"

class UButton;
class UTexture2D;
class UWidget;

UCLASS(Abstract)
class TIMETHIEF_API UMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UWidget* GetInitialFocusWidget() const;

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> MatchAction_Button;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ExitGame_Button;

	// === 버튼 상태별 텍스처 (에디터에서 할당) ===
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Matching")
	TObjectPtr<UTexture2D> Tex_MatchStart;       // 대기: 매칭 시작

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Matching")
	TObjectPtr<UTexture2D> Tex_MatchStartHover;  // 대기+오버: 매칭 시작 (하이라이트)

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Matching")
	TObjectPtr<UTexture2D> Tex_Matching;         // 매칭중: 매칭 중 

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Matching")
	TObjectPtr<UTexture2D> Tex_MatchCancelHover; // 매칭중+오버: 매칭 취소 

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION() void OnMatchActionClicked();
	UFUNCTION() void OnExitGameClicked();

	UFUNCTION() void HandlePlayStateChanged(ENetworkPlayState NewState);

private:
	void RefreshMatchButtonVisuals();

	ENetworkPlayState CachedPlayState = ENetworkPlayState::Disconnected;
};
