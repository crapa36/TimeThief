// Fill out your copyright notice in the Description page of Project Settings.


#include "ItemSlotWidgetBase.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "DataAssets/GameItemData.h"
#include "Game/ItemSettings.h"

void UItemSlotWidgetBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (TooltipWidgetClass && ItemTooltipWidget == nullptr)
	{
		ItemTooltipWidget = CreateWidget<UItemToolTipWidget>(GetWorld(), TooltipWidgetClass);
		ItemTooltipWidget->SetAlignmentInViewport(FVector2D(0, 0));
		ItemTooltipWidget->AddToViewport(100);
		ItemTooltipWidget->SetVisibility(ESlateVisibility::Hidden);
	}

	if (Slot_Button)
	{
		Slot_Button->OnClicked.AddUniqueDynamic(this, &ThisClass::OnSlotClicked);
	}

	UpdateItem(ItemID);
}

void UItemSlotWidgetBase::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);

	if (ItemTooltipWidget)
	{
		ItemTooltipWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UItemSlotWidgetBase::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);

	if (ItemTooltipWidget)
	{
		ItemTooltipWidget->SetVisibility(ESlateVisibility::Hidden);
	}
}

void UItemSlotWidgetBase::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (ItemTooltipWidget && ItemTooltipWidget->IsVisible())
	{
		FVector2D MousePos;

		UWidgetLayoutLibrary::GetMousePositionScaledByDPI(GetOwningPlayer(), MousePos.X, MousePos.Y);

		ItemTooltipWidget->SetPositionInViewport(MousePos + ToolTipOffset, false);
	}
}

void UItemSlotWidgetBase::NativeDestruct()
{
	Super::NativeDestruct();
	
	if (ItemTooltipWidget)
	{
		ItemTooltipWidget->SetVisibility(ESlateVisibility::Hidden);
	}
}

void UItemSlotWidgetBase::UpdateItem(EItemID InItemID)
{
	ItemID = InItemID;

	UpdateUI();
}

void UItemSlotWidgetBase::UpdateUI()
{
	const UItemSettings* StoreSettings = GetDefault<UItemSettings>();
	if (UGameItemData* LoadedData = StoreSettings->ItemData.LoadSynchronous())
	{
		const FItemData& ItemStat = LoadedData->Items[ItemID];

		if (ItemIcon_Image)
		{
			ItemIcon_Image->SetBrushFromTexture(ItemStat.Icon);
		}

		if (ItemName_Text)
		{
			ItemName_Text->SetText(FText::FromString(ItemStat.Name));
		}

		if (ItemTooltipWidget)
		{
			ItemTooltipWidget->SetupToolTip(ItemID);
		}
	}
}
