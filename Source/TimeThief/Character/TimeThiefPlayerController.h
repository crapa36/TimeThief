#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Components/GameFrameworkInitStateInterface.h"
#include "Network/State/NetworkPlayState.h"
#include "TimeThiefPlayerController.generated.h"

class UInputMappingContext;
class UTimeThiefInputConfig;
class UTimeThiefHUDWidget;
class UMainMenuWidget;
class UUserWidget;

UENUM(BlueprintType)
enum class EWidgetType : uint8 {
	Inventory,
	Store,
	Minimap,
	WheelMenu,
	SIZE,
	None
};

UCLASS()
class TIMETHIEF_API ATimeThiefPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	ATimeThiefPlayerController();

	UFUNCTION(BlueprintCallable, Category = "TimeThief|UI")
	void ToggleWidget(EWidgetType WidgetType);

	UFUNCTION(BlueprintCallable, Category = "TimeThief|UI")
	void SetVisibilityWidget(EWidgetType WidgetType, bool bVisible);

	UFUNCTION(BlueprintPure, Category = "TimeThief|UI")
	UTimeThiefHUDWidget* GetHUDWidget() const { return MainHUDWidget; }

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void SetPawn(APawn* InPawn) override;
	virtual void OnPossess(APawn* InPawn) override;
	
	void OnPawnInitStateChanged(const FActorInitStateChangedParams& Params);
	void InitializeUI();

	void ShowMainMenu();
	void HideMainMenu();

	UFUNCTION()
	void HandleNetworkPlayStateChanged(ENetworkPlayState NewState);

	UPROPERTY(EditAnywhere, Category = "Input|Input Mappings")
	TArray<TObjectPtr<UInputMappingContext>> DefaultMappingContexts;

	UPROPERTY(EditAnywhere, Category = "UI|HUD")
	TSubclassOf<UTimeThiefHUDWidget> MainHUDWidgetClass;

	UPROPERTY(EditAnywhere, Category = "UI|MainMenu")
	TSubclassOf<UMainMenuWidget> MainMenuWidgetClass;

	UPROPERTY(EditAnywhere, Category = "UI")
	TMap<EWidgetType, TSubclassOf<UUserWidget>> SubWidgetClassMap;

	UPROPERTY()
	TArray<TObjectPtr<UUserWidget>> SubWidgets;

	UPROPERTY()
	TObjectPtr<UTimeThiefHUDWidget> MainHUDWidget;

	UPROPERTY()
	TObjectPtr<UMainMenuWidget> MainMenuWidget;

	bool bUIInitialized = false;
};