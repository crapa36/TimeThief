#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TimeThiefPlayerController.generated.h"

class UStoreWidget;
class UMinimapWidget;
class UInputMappingContext;
class UUserWidget;
class UTimeThiefHUDWidget;

UCLASS(abstract)
class TIMETHIEF_API ATimeThiefPlayerController : public APlayerController
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(EditAnywhere, Category ="Input|Input Mappings")
	TArray<TObjectPtr<UInputMappingContext>> DefaultMappingContexts;

	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<TObjectPtr<UInputMappingContext>> MobileExcludedMappingContexts;

	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	UPROPERTY(EditAnywhere, Category = "UI|HUD")
	TSubclassOf<UTimeThiefHUDWidget> MainHUDWidgetClass;
	
	UPROPERTY(EditAnywhere, Category = "UI|Minimap")
	TSubclassOf<UMinimapWidget> MinimapWidgetClass;
	
	UPROPERTY(EditAnywhere, Category = "UI|Store")
	TSubclassOf<UStoreWidget> StoreWidgetClass;
	
	UPROPERTY()
	TObjectPtr<UTimeThiefHUDWidget> MainHUDWidget;
	
	UPROPERTY()
	TObjectPtr<UMinimapWidget> MinimapWidget;
	
	UPROPERTY()
	TObjectPtr<UStoreWidget> StoreWidget;
	
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	bool ShouldUseTouchControls() const;
	
	bool IsShowingMinimap = false;
	
public:
	void ToggleMinimap();
	
	void SetStoreVisibility(bool bVisible);
};