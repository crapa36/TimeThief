// Fill out your copyright notice in the Description page of Project Settings.


#include "StoreCategoryWidget.h"

#include "Components/HorizontalBox.h"
#include "StoreSlotWidget.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Game/ItemSettings.h"

void UStoreCategoryWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (Items_HorizontalBox)
	{
		Items_HorizontalBox->ClearChildren();
	}
	
	if (!SlotClass)
	{
		return; 
	}
	const UItemSettings* StoreSettings = GetDefault<UItemSettings>();

	if (UStoreCategoryData* LoadedData = StoreSettings->CategoryData.LoadSynchronous())
	{
		if (!LoadedData->StoreItemMap.Contains(ItemType))
		{
			return;
		}
		if (Category_Text)
		{
			Category_Text->SetText(FText::FromString(LoadedData->StoreItemMap[ItemType].CategoryName));
		}
		for (EItemID ItemName : LoadedData->StoreItemMap[ItemType].ItemList)
		{
			auto NewWidget = CreateWidget<UStoreSlotWidget>(this, SlotClass);
			NewWidget->Init(ItemName);
			UHorizontalBoxSlot* HBSlot = Cast<UHorizontalBoxSlot>(Items_HorizontalBox->AddChild(NewWidget));
			HBSlot->SetPadding(FMargin{2.5, 5, 2.5, 0});
		}
	}
}
