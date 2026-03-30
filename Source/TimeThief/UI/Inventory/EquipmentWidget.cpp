// Fill out your copyright notice in the Description page of Project Settings.


#include "EquipmentWidget.h"

#include "Character/TimeThiefPlayerCharacter.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/System/InventorySystemComponent.h"
#include "Game/ItemSettings.h"
#include "UI/PromptWidget.h"

void UEquipmentWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	if (auto Player = Cast<ATimeThiefPlayerCharacter>(GetOwningPlayerPawn()))
	{
		if (auto Inventory = Player->GetInventoryComponent())
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
}

void UEquipmentWidget::NativeDestruct()
{
	Super::NativeDestruct();
	
	if (auto Player = Cast<ATimeThiefPlayerCharacter>(GetOwningPlayerPawn()))
	{
		if (auto Inventory = Player->GetInventoryComponent())
		{
			Inventory->OnThrowableEquipmentUpdatedEvent.RemoveAll(this);
			Inventory->OnConsumableEquipmentUpdatedEvent.RemoveAll(this);
		}
	}
}

void UEquipmentWidget::OnChangedEquipment(EItemID InItemID)
{
	if (InItemID == EItemID::SIZE)
	{
		auto Brush = ItemIcon_Image->GetBrush();
		Brush.DrawAs = ESlateBrushDrawType::NoDrawType;
		ItemIcon_Image->SetBrush(Brush);
		
		PromptWidget->SetVisibility(ESlateVisibility::Hidden);
		return;
	}
	const UItemSettings* StoreSettings = GetDefault<UItemSettings>();
	if (UGameItemData* LoadedData = StoreSettings->ItemData.LoadSynchronous())
	{
		ItemIcon_Image->SetBrushFromTexture(LoadedData->Items[InItemID].Icon);
		auto Brush = ItemIcon_Image->GetBrush();
		Brush.DrawAs = ESlateBrushDrawType::Image;
		ItemIcon_Image->SetBrush(Brush);
		PromptWidget->Prompt_Text->SetText(FText::FromString(LoadedData->Items[InItemID].Name));
		PromptWidget->SetVisibility(ESlateVisibility::Visible);
	}
}

