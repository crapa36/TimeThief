// Fill out your copyright notice in the Description page of Project Settings.


#include "EquipmentWidget.h"

#include "Character/TimeThiefPlayerCharacter.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/System/InventorySystemComponent.h"
#include "Game/ItemSettings.h"
#include "UI/PromptWidget.h"

void UEquipmentWidget::Init(ATimeThiefPlayerCharacter* InPlayer)
{
	if (BoundInventoryComponent.IsValid())
	{
		BoundInventoryComponent->OnThrowableEquipmentUpdatedEvent.RemoveAll(this);
		BoundInventoryComponent->OnConsumableEquipmentUpdatedEvent.RemoveAll(this);
		BoundInventoryComponent.Reset();
	}

	if (!InPlayer)
	{
		OnChangedEquipment(EItemID::SIZE);
		return;
	}

	if (auto Inventory = InPlayer->GetInventoryComponent())
	{
		BoundInventoryComponent = Inventory;
		if (EquipmentCategory == EItemCategory::Throwable)
		{
			Inventory->OnThrowableEquipmentUpdatedEvent.AddUObject(this, &ThisClass::OnChangedEquipment);
			OnChangedEquipment(Inventory->GetThrowableEquipment());
			PromptWidget->ActionKey_Text->SetText(FText::FromString(TEXT("5")));
		}
		else if (EquipmentCategory == EItemCategory::Consumable)
		{
			Inventory->OnConsumableEquipmentUpdatedEvent.AddUObject(this, &ThisClass::OnChangedEquipment);
			OnChangedEquipment(Inventory->GetConsumableEquipment());
			PromptWidget->ActionKey_Text->SetText(FText::FromString(TEXT("4")));
		}
	}
}

void UEquipmentWidget::OnChangedEquipment(EItemID InItemID)
{
	if (InItemID == EItemID::SIZE)
	{
		SetVisibility(ESlateVisibility::Hidden);
		return;
	}
	const UItemSettings* StoreSettings = GetDefault<UItemSettings>();
	if (UGameItemData* LoadedData = StoreSettings->ItemData.LoadSynchronous())
	{
		const FItemData* ItemData = LoadedData->Items.Find(InItemID);
		if (!ItemData)
		{
			SetVisibility(ESlateVisibility::Hidden);
			return;
		}

		SetVisibility(ESlateVisibility::Visible);
		ItemIcon_Image->SetBrushFromTexture(ItemData->Icon);
		PromptWidget->Prompt_Text->SetText(FText::FromString(ItemData->Name));
	}
}

