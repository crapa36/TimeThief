// Fill out your copyright notice in the Description page of Project Settings.


#include "StoreSlotWidget.h"

#include "Character/TimeThiefCharacterBase.h"
#include "Character/TimeThiefPlayerCharacter.h"
#include "Character/TimeThiefPlayerState.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Game/ItemSettings.h"

void UStoreSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UStoreSlotWidget::OnSlotClicked()
{
	UGameItemData* ItemData = GetDefault<UItemSettings>()->ItemData.LoadSynchronous();

	if (ATimeThiefPlayerCharacter* Player = Cast<ATimeThiefPlayerCharacter>(GetOwningPlayerPawn()))
	{
		if (const ATimeThiefPlayerState* PS = Cast<ATimeThiefPlayerState>(Player->GetPlayerState()))
		{
			const FItemData& ItemInfo = GetDefault<UItemSettings>()->ItemData->Items[ItemID];
			FStoreOrder Order;
			Order.ItemID = ItemID;
			Order.Price = ItemInfo.Price;
			int Level = 0;
			switch (ItemID)
			{
			case EItemID::DamageUpgrade:
				Level = PS->Status.Damage;
				break;
			case EItemID::StabilityUpgrade:
				Level = PS->Status.Stability;
				break;
			case EItemID::CapacityUpgrade:
				Level = PS->Status.Capacity;
				break;
			case EItemID::HealthUpgrade:
				Level = PS->Status.Health;
				break;
			case EItemID::SpeedUpgrade:
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

void UStoreSlotWidget::Init(EItemID InItemID)
{
	UpdateItem(InItemID);

	UpdateUI();
}

void UStoreSlotWidget::UpdateUI()
{
	Super::UpdateUI();
	
	const UItemSettings* StoreSettings = GetDefault<UItemSettings>();
	if (UGameItemData* LoadedData = StoreSettings->ItemData.LoadSynchronous())
	{
		const FItemData& ItemStat = LoadedData->Items[ItemID];

		if (Price_Text)
		{
			int Price = ItemStat.Price;
			if (ATimeThiefCharacterBase* Player = Cast<ATimeThiefCharacterBase>(GetOwningPlayerPawn()))
			{
				if (ATimeThiefPlayerState* PS = Cast<ATimeThiefPlayerState>(Player->GetPlayerState()))
				{
					Price += ItemStat.Increment * [&]()
					{
						switch (ItemID)
						{
						case EItemID::DamageUpgrade:
							return PS->Status.Damage;
						case EItemID::StabilityUpgrade:
							return PS->Status.Stability;
						case EItemID::CapacityUpgrade:
							return PS->Status.Capacity;
						case EItemID::HealthUpgrade:
							return PS->Status.Health;
						case EItemID::SpeedUpgrade:
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
