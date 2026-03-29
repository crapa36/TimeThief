// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryWidget.h"

#include "Character/TimeThiefPlayerCharacter.h"
#include "Components/ListView.h"
#include "Actors/Item/ItemBase.h"
#include "Components/System/InventorySystemComponent.h"
#include "Kismet/KismetSystemLibrary.h"

void UInventoryWidget::OnVicinityItemUpdated()
{
	if (IsVisible() && Player.IsValid())
	{
		VicinityItem_ListView->SetListItems(Player->GetVicinityItems());
	}
}

void UInventoryWidget::OnInventoryItemUpdated()
{
	TArray<TObjectPtr<UInventoryObject>> InventoryItems;
	
	
	for (UInventoryObject* Object : Player->GetInventoryComponent()->GetInventory())
	{
		if (Object->Quantity > 0)
		{
			InventoryItems.Emplace(Object);
		}
	}
	
	UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Inventory Updated: %d"), InventoryItems.Num()));
	
	Inventory_ListView->SetListItems(InventoryItems);
}

void UInventoryWidget::Init(ATimeThiefPlayerCharacter* InPlayer)
{
	Player = InPlayer;
	Player->OnVicinityItemUpdatedEvent.AddUObject(this, &ThisClass::OnVicinityItemUpdated);
	Player->GetInventoryComponent()->OnInventoryUpdatedEvent.AddUObject(this, &ThisClass::OnInventoryItemUpdated);
}
