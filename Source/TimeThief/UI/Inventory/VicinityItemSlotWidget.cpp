// Fill out your copyright notice in the Description page of Project Settings.


#include "VicinityItemSlotWidget.h"


#include "Components/TextBlock.h"
#include "Actors//Item/ItemBase.h"
#include "Character/TimeThiefPlayerCharacter.h"
#include "Components/Border.h"
#include "Game/ItemSettings.h"
#include "Kismet/KismetSystemLibrary.h"

void UVicinityItemSlotWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	
	ItemBackground_Border->SetContentColorAndOpacity(OnMouseColor);
	ItemBackground_Border->SetBrushColor(OnMouseColor);
	Interaction_Text->SetVisibility(ESlateVisibility::Visible);
}

void UVicinityItemSlotWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	
	ItemBackground_Border->SetContentColorAndOpacity(FLinearColor::White);
	ItemBackground_Border->SetBrushColor(OriginColor);
	Interaction_Text->SetVisibility(ESlateVisibility::Hidden);
}

void UVicinityItemSlotWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);
	
	if (AItemBase* Item = Cast<AItemBase>(ListItemObject))
	{
		BindItem(Item);
	}
	
	ItemBackground_Border->SetBrushColor(OriginColor);
}

void UVicinityItemSlotWidget::UpdateUI()
{
	Super::UpdateUI();
	
	if (BindedItem.IsValid())
	{
		ItemQuantity_Text->SetText(FText::AsNumber(BindedItem->GetQuantity()));
	}
}

void UVicinityItemSlotWidget::OnSlotClicked()
{
	if (BindedItem.IsValid())
	{
		BindedItem->Interact(Cast<ATimeThiefPlayerCharacter>(GetOwningPlayerPawn()));
	}
}

void UVicinityItemSlotWidget::BindItem(AItemBase* InItemActor)
{
	BindedItem = InItemActor;
	
	ItemID = BindedItem->GetItemID();
	
	const UItemSettings* StoreSettings = GetDefault<UItemSettings>();
	if (UGameItemData* LoadedData = StoreSettings->ItemData.LoadSynchronous())
	{
		if (ItemID == EItemID::TimePoint ||
			LoadedData->Items[ItemID].Category != EItemCategory::Consumable )
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



