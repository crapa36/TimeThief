// Fill out your copyright notice in the Description page of Project Settings.


#include "ItemSlotWidget.h"

void UItemSlotWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	
	if (TooltipWidgetClass && CachedTooltipWidget == nullptr)
	{
		CachedTooltipWidget = CreateWidget<UItemToolTipWidget>(GetWorld(), TooltipWidgetClass);
	}
	
	ToolTipWidgetDelegate.BindDynamic(this, &UItemSlotWidget::GenerateTooltipWidget);
	
	TooltipData.Name = TEXT("대용량 탄창");
	TooltipData.Description = TEXT("탄창의 용량이 증가합니다.");
}

UWidget* UItemSlotWidget::GenerateTooltipWidget()
{
	if (CachedTooltipWidget)
	{
		CachedTooltipWidget->SetupTooltip(TooltipData);
	}
	
	return CachedTooltipWidget;
}
