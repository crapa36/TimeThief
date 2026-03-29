#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TimeThiefPlayerController.generated.h"

class UInventoryWidget;
class UStoreWidget;
class UMinimapWidget;
class UInputMappingContext;
class UUserWidget;
class UTimeThiefHUDWidget;

UENUM(BlueprintType)
enum class EWidgetType : uint8
{
	Minimap = 0,
	Store,
	Inventory,
	SIZE
};

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
	
	UPROPERTY(EditAnywhere, Category = "UI")
	TMap<EWidgetType, TSubclassOf<UUserWidget>> SubWidgetClassMap;
	
	UPROPERTY()
	TArray<TObjectPtr<UUserWidget>> SubWidgets;
	
	UPROPERTY()
	TObjectPtr<UTimeThiefHUDWidget> MainHUDWidget;
	
	ATimeThiefPlayerController();
	
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void SetPawn(APawn* InPawn) override;
	virtual void OnPossess(APawn* InPawn) override;

	bool ShouldUseTouchControls() const;
	
public:
	void ToggleWidget(EWidgetType WidgetType);
	void SetVisibilityWidget(EWidgetType WidgetType, bool bVisible);
};