// Fill out your copyright notice in the Description page of Project Settings.


#include "StoreSlotWidget.h"

#include "Character/TimeThiefCharacterBase.h"
#include "Character/TimeThiefPlayerState.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Game/StoreSettings.h"

void UStoreSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Slot_Button)
	{
		Slot_Button->OnClicked.AddUniqueDynamic(this, &UStoreSlotWidget::OnSlotClicked);
	}
}

void UStoreSlotWidget::OnSlotClicked()
{
	if (ATimeThiefCharacterBase* Player = Cast<ATimeThiefCharacterBase>(GetOwningPlayerPawn()))
	{
		if (const ATimeThiefPlayerState* PS = Cast<ATimeThiefPlayerState>(Player->GetPlayerState()))
		{
			const FStoreItemInfo& ItemInfo = GetDefault<UStoreSettings>()->ItemData->StoreItems[ItemName];
			FStoreOrder Order;
			Order.ItemName = ItemName;
			Order.Price = ItemInfo.Price;
			int Level = 0;
			switch (ItemName)
			{
			case EStoreItemName::DamageUpgrade:
				Level = PS->Status.Damage;
				break;
			case EStoreItemName::StabilityUpgrade:
				Level = PS->Status.Stability;
				break;
			case EStoreItemName::CapacityUpgrade:
				Level = PS->Status.Capacity;
				break;
			case EStoreItemName::HealthUpgrade:
				Level = PS->Status.Health;
				break;
			case EStoreItemName::SpeedUpgrade:
				Level = PS->Status.Speed;
				break;
			default:
				break;
			}
			Order.Price += Level * ItemInfo.Increment;
			Player->PurchaseItem(Order);
			
			UpdateUI();
		}
	}
}

void UStoreSlotWidget::Init(EStoreItemName InItemName)
{
	ItemName = InItemName;

	UpdateUI();
}

void UStoreSlotWidget::UpdateUI()
{
	const UStoreSettings* StoreSettings = GetDefault<UStoreSettings>();
	if (UStoreItemData* LoadedData = StoreSettings->ItemData.LoadSynchronous())
	{
		const FStoreItemInfo& ItemStat = LoadedData->StoreItems[ItemName];

		if (Item_Image)
		{
			Item_Image->SetBrushFromTexture(ItemStat.Icon);
		}

		if (Item_Text)
		{
			Item_Text->SetText(FText::FromString(ItemStat.Name));
		}

		if (Price_Text)
		{
			int Price = ItemStat.Price;
			if (ATimeThiefCharacterBase* Player = Cast<ATimeThiefCharacterBase>(GetOwningPlayerPawn()))
			{
				if (ATimeThiefPlayerState* PS = Cast<ATimeThiefPlayerState>(Player->GetPlayerState()))
				{
					Price += ItemStat.Increment * [&]()
					{
						switch (ItemName)
						{
						case EStoreItemName::DamageUpgrade:
							return PS->Status.Damage;
						case EStoreItemName::StabilityUpgrade:
							return PS->Status.Stability;
						case EStoreItemName::CapacityUpgrade:
							return PS->Status.Capacity;
						case EStoreItemName::HealthUpgrade:
							return PS->Status.Health;
						case EStoreItemName::SpeedUpgrade:
							return PS->Status.Speed;
						default:
							return 0;
						}
					}();
				}
			}
			Price_Text->SetText(FText::AsNumber(Price));
		}
	}
}
