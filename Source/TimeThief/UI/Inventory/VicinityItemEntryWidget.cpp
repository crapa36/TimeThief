// Fill out your copyright notice in the Description page of Project Settings.


#include "VicinityItemEntryWidget.h"

#include "Actors/Item/ItemBase.h"
#include "Character/TimeThiefPlayerCharacter.h"
#include "Components/TextBlock.h"
#include "Game/ItemSettings.h"

void UVicinityItemEntryWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	Super::NativeOnListItemObjectSet(ListItemObject);
	
	if (AItemBase* Item = Cast<AItemBase>(ListItemObject))
	{
		BindItem(Item);
	}
}

void UVicinityItemEntryWidget::UpdateUI()
{
	Super::UpdateUI();
	
	if (BindedItem.IsValid())
	{
		ItemQuantity_Text->SetText(FText::AsNumber(BindedItem->GetQuantity()));
	}
}

void UVicinityItemEntryWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	
	Interaction_Text->SetVisibility(ESlateVisibility::Visible);
}

void UVicinityItemEntryWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	
	Interaction_Text->SetVisibility(ESlateVisibility::Hidden);
}

void UVicinityItemEntryWidget::OnSlotClicked()
{
	Super::OnSlotClicked();
	
	if (BindedItem.IsValid())
	{
		BindedItem->Interact(Cast<ATimeThiefPlayerCharacter>(GetOwningPlayerPawn()));
	}
}

void UVicinityItemEntryWidget::BindItem(AItemBase* InItemActor)
{
	BindedItem = InItemActor;
	
	ItemID = BindedItem->GetItemID();

	if (UGameItemData* LoadedData = GetDefault<UItemSettings>()->GetItemData())
	{
		if (LoadedData->Items[ItemID].Category != EItemCategory::Consumable )
		{
			Interaction_Text->SetText(FText::FromString(TEXT("사용")));
		}
		else
		{
			Interaction_Text->SetText(FText::FromString(TEXT("줍기")));
		}
	}

	UpdateUI();
}
