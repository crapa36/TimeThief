// Fill out your copyright notice in the Description page of Project Settings.


#include "ItemEntryWidgetBase.h"


#include "Components/Border.h"

void UItemEntryWidgetBase::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	
	ItemBackground_Border->SetContentColorAndOpacity(OnMouseColor);
	ItemBackground_Border->SetBrushColor(OnMouseColor);
}

void UItemEntryWidgetBase::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	
	ItemBackground_Border->SetContentColorAndOpacity(FLinearColor::White);
	ItemBackground_Border->SetBrushColor(OriginColor);
}

void UItemEntryWidgetBase::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);
	
	ItemBackground_Border->SetBrushColor(OriginColor);
}




