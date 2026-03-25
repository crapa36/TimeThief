// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ItemToolTipWidget.h"
#include "ItemSlotWidget.generated.h"

/**
 * 
 */
class UButton;
class UTextBlock;
class UImage;

UCLASS()
class TIMETHIEF_API UItemSlotWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Slot_Button;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Item_Image;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Item_Text;

protected:
	virtual void NativeOnInitialized() override;
	
public:
	UFUNCTION()
	UWidget* GenerateTooltipWidget();

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Tooltip")
	TSubclassOf<UItemToolTipWidget> TooltipWidgetClass;
	
	UPROPERTY()
	TObjectPtr<UItemToolTipWidget> CachedTooltipWidget;

	FItemToolTipData TooltipData;
};
