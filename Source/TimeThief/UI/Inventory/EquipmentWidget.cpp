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
	if (auto Inventory = InPlayer->GetInventoryComponent())
	{
		if (EquipmentCategory == EItemCategory::Throwable)
		{
			Inventory->OnThrowableEquipmentUpdatedEvent.AddUObject(this, &ThisClass::OnChangedEquipment);
			OnChangedEquipment(EItemID::SIZE);
			PromptWidget->ActionKey_Text->SetText(FText::FromString(TEXT("5")));
		}
		else if (EquipmentCategory == EItemCategory::Consumable)
		{
			Inventory->OnConsumableEquipmentUpdatedEvent.AddUObject(this, &ThisClass::OnChangedEquipment);
			OnChangedEquipment(EItemID::SIZE);
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
		SetVisibility(ESlateVisibility::Visible);
		ItemIcon_Image->SetBrushFromTexture(LoadedData->Items[InItemID].Icon);
		PromptWidget->Prompt_Text->SetText(FText::FromString(LoadedData->Items[InItemID].Name));
	}
}

