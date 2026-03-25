// Fill out your copyright notice in the Description page of Project Settings.


#include "ItemToolTipWidget.h"

#include "Components/TextBlock.h"

void UItemToolTipWidget::SetupTooltip(const FItemToolTipData& Tooltip)
{
	if (ItemName_Text && ItemDesc_Text)
	{
		ItemName_Text->SetText(FText::FromString(Tooltip.Name));
		ItemDesc_Text->SetText(FText::FromString(Tooltip.Description));
	}
}
