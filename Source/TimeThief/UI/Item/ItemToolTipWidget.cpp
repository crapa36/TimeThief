// Fill out your copyright notice in the Description page of Project Settings.


#include "ItemToolTipWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Game/ItemSettings.h"
#include <map>

std::map<EItemCategory, FString> CategoryNameMap = {
	{EItemCategory::CharacterUpgrade, TEXT("강화 파츠")},
	{EItemCategory::WeaponUpgrade, TEXT("강화 파츠")},
	{EItemCategory::Skill, TEXT("스킬")},
	{EItemCategory::Consumable, TEXT("소모품")},
	{EItemCategory::Throwable, TEXT("투척 무기")},
};

void UItemToolTipWidget::SetupToolTip(EItemID InItemID)
{
	const UItemSettings* ItemSettings = GetDefault<UItemSettings>();

	if (UGameItemData* LoadedData = ItemSettings->ItemData.LoadSynchronous())
	{
		ItemName_Text->SetText(FText::FromString(LoadedData->Items[InItemID].Name));
		ItemDesc_Text->SetText(FText::FromString(
		LoadedData->Items[InItemID].Description.ReplaceEscapedCharWithChar()));

		Item_Image->SetBrushFromTexture(LoadedData->Items[InItemID].Icon);

		CategoryName_Text->SetText(FText::FromString(CategoryNameMap[LoadedData->Items[InItemID].Category]));
		
		Stat_Text->SetText(FText::FromString(LoadedData->Items[InItemID].Stat));
	}
}
