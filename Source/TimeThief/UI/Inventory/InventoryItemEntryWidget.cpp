// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryItemEntryWidget.h"

#include "Character/TimeThiefPlayerCharacter.h"
#include "Components/TextBlock.h"
#include "Components/System/InventorySystemComponent.h"
#include "Kismet/KismetSystemLibrary.h"

void UInventoryItemEntryWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	Super::NativeOnListItemObjectSet(ListItemObject);
	
	if (auto ItemObject = Cast<UInventoryObject>(ListItemObject))
	{
		BindItem(ItemObject);
	}
}

void UInventoryItemEntryWidget::UpdateUI()
{
	Super::UpdateUI();
	
	if (Item.IsValid())
	{
		ItemQuantity_Text->SetText(FText::AsNumber(Item->Quantity));
	}
}

void UInventoryItemEntryWidget::OnSlotClicked()
{
	Super::OnSlotClicked();
	
	if (Item.IsValid())
	{
		if (auto Player = Cast<ATimeThiefPlayerCharacter>(GetOwningPlayerPawn()))
		{
			Player->GetInventoryComponent()->RemoveItem(ItemID, 1);
		}
	}
}

void UInventoryItemEntryWidget::OnInventoryObjectUpdated()
{
	if (Item.IsValid())
	{
		ItemQuantity_Text->SetText(FText::AsNumber(Item->Quantity));
	}
	
	UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Item Updated: %d"), Item->Quantity));
}

void UInventoryItemEntryWidget::BindItem(UInventoryObject* InItem)
{
	if (Item.IsValid())
	{
		Item->OnInventoryObjectUpdatedEvent.RemoveAll(this);
	}
	
	Item = InItem;
	Item->OnInventoryObjectUpdatedEvent.AddUObject(this, &ThisClass::OnInventoryObjectUpdated);
	ItemID = InItem->ItemID;
	
	UpdateUI();
}
