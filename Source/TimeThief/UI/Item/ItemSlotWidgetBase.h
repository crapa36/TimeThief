// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ItemCommons.h"
#include "Blueprint/UserWidget.h"
#include "ItemToolTipWidget.h"
#include "ItemSlotWidgetBase.generated.h"

/**
 * 
 */
class UButton;
class UTextBlock;
class UImage;

UCLASS()
class TIMETHIEF_API UItemSlotWidgetBase : public UUserWidget
{
	GENERATED_BODY()

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Slot_Button;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> ItemIcon_Image;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ItemName_Text;

protected:
	virtual void NativeOnInitialized() override;
	
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	
public:
	void UpdateItem(EItemID InItemID);
	
protected:
	UPROPERTY(EditDefaultsOnly, Category = "Tooltip")
	TSubclassOf<UItemToolTipWidget> TooltipWidgetClass;
	
	UPROPERTY()
	TObjectPtr<UItemToolTipWidget> ItemTooltipWidget;
	
	UPROPERTY(EditAnywhere)
	EItemID ItemID;
	
	UPROPERTY(EditAnywhere, Category = "Tooltip")
	FVector2D ToolTipOffset;
};
