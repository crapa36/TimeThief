// Fill out your copyright notice in the Description page of Project Settings.


#include "ItemSlotWidgetBase.h"

void UItemSlotWidgetBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (TooltipWidgetClass && ItemTooltipWidget == nullptr)
	{
		ItemTooltipWidget = CreateWidget<UItemToolTipWidget>(GetWorld(), TooltipWidgetClass);
		ItemTooltipWidget->SetAlignmentInViewport(FVector2D(0, 0));
		ItemTooltipWidget->AddToViewport(100);
		ItemTooltipWidget->SetVisibility(ESlateVisibility::Hidden);
	}
	
	UpdateItem(ItemID);
}

void UItemSlotWidgetBase::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);

	if (ItemTooltipWidget)
	{
		ItemTooltipWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UItemSlotWidgetBase::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);

	if (ItemTooltipWidget)
	{
		ItemTooltipWidget->SetVisibility(ESlateVisibility::Hidden);
	}
}

void UItemSlotWidgetBase::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (ItemTooltipWidget && ItemTooltipWidget->IsVisible())
	{
		FVector2D MousePos = FSlateApplication::Get().GetCursorPos();
	
		ItemTooltipWidget->SetPositionInViewport(MousePos + ToolTipOffset);
	}
}

void UItemSlotWidgetBase::UpdateItem(EItemID InItemID)
{
	ItemID = InItemID;

	if (ItemTooltipWidget)
	{
		ItemTooltipWidget->SetupToolTip(ItemID);
	}
}
