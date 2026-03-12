#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TimeThiefPlayerController.generated.h"

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

	UPROPERTY()
	TObjectPtr<UTimeThiefHUDWidget> MainHUDWidget;

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	bool ShouldUseTouchControls() const;
};