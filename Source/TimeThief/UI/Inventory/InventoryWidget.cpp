// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryWidget.h"

#include "Character/TimeThiefPlayerCharacter.h"
#include "Components/ListView.h"
#include "Actors/Item/ItemBase.h"
#include "Components/VerticalBox.h"
#include "Components/System/InventorySystemComponent.h"
#include "Kismet/KismetSystemLibrary.h"

void UInventoryWidget::OnVicinityItemUpdated()
{
	if (IsVisible() && Player.IsValid())
	{
		const auto VicinityItems = Player->GetVicinityItems();
		if (VicinityItems.Num() > 0)
		{
			VicinityItem_ListView->SetListItems(VicinityItems);
			Vicinity_VerticalBox->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			Vicinity_VerticalBox->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

void UInventoryWidget::OnInventoryItemUpdated()
{
	if (!Player.IsValid() || !Player->GetInventoryComponent())
	{
		return;
	}

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

void UInventoryWidget::SetVisibility(ESlateVisibility InVisibility)
{
	Super::SetVisibility(InVisibility);
	
	if (InVisibility == ESlateVisibility::Hidden)
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			PC->SetIgnoreLookInput(false);
			PC->SetInputMode(FInputModeGameOnly{});
			PC->bShowMouseCursor = false;
		}
	}
	else if (InVisibility == ESlateVisibility::Visible)
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			PC->SetIgnoreLookInput(true);

			PC->SetInputMode(FInputModeGameAndUI{});
			PC->bShowMouseCursor = true;

			int32 ViewportSizeX, ViewportSizeY;
			PC->GetViewportSize(ViewportSizeX, ViewportSizeY);
			PC->SetMouseLocation(ViewportSizeX / 2, ViewportSizeY / 2);
		}
	}
}

void UInventoryWidget::Init(ATimeThiefPlayerCharacter* InPlayer)
{
	Player = InPlayer;

	if (Player.IsValid())
	{
		Player->OnVicinityItemUpdatedEvent.AddUObject(this, &ThisClass::OnVicinityItemUpdated);
		
		if (UInventorySystemComponent* InventoryComp = Player->GetInventoryComponent())
		{
			InventoryComp->OnInventoryUpdatedEvent.AddUObject(this, &ThisClass::OnInventoryItemUpdated);
		}
	}
}