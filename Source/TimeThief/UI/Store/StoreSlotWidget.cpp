// Fill out your copyright notice in the Description page of Project Settings.


#include "StoreSlotWidget.h"

#include "Character/TimeThiefCharacterBase.h"
#include "Character/TimeThiefPlayerCharacter.h"
#include "Character/TimeThiefPlayerController.h"
#include "Character/TimeThiefPlayerState.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Game/ItemSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Network/NetworkGameInstanceSubsystem.h"
#include "Sound/SoundBase.h"

void UStoreSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UNetworkGameInstanceSubsystem* NGIS = UNetworkGameInstanceSubsystem::Get(this))
	{
		NGIS->OnStorePurchaseSucceeded.RemoveAll(this);
		NGIS->OnStorePurchaseSucceeded.AddUObject(this, &ThisClass::OnStorePurchaseSucceeded);
		NGIS->OnStorePriceDataUpdated.RemoveAll(this);
		NGIS->OnStorePriceDataUpdated.AddUObject(this, &ThisClass::OnStorePriceDataUpdated);
	}
}

void UStoreSlotWidget::NativeDestruct()
{
	if (UNetworkGameInstanceSubsystem* NGIS = UNetworkGameInstanceSubsystem::Get(this))
	{
		NGIS->OnStorePurchaseSucceeded.RemoveAll(this);
		NGIS->OnStorePriceDataUpdated.RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UStoreSlotWidget::OnSlotClicked()
{
	if (auto* NGIS = UNetworkGameInstanceSubsystem::Get(this))
	{
		if (!NGIS->IsConnected())
		{
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
					const bool bPurchased = Player->PurchaseItem(Order);
					if (bPurchased)
					{
						PlayPurchaseSuccessSound();
					}
			
					UpdateUI();
				}
			}
		}
		else
		{
			if (NGIS->IsStoreItemSoldOut(static_cast<uint32>(ItemID)))
			{
				return;
			}

			uint32 StoreEntityId = 0;
			if (ATimeThiefPlayerCharacter* Player = Cast<ATimeThiefPlayerCharacter>(GetOwningPlayerPawn()))
			{
				if (ATimeThiefPlayerController* PC = Cast<ATimeThiefPlayerController>(Player->GetController()))
				{
					StoreEntityId = PC->GetLastInteractedStoreId();
				}	
			}
			
			if (StoreEntityId == 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("StoreSlotWidget::OnSlotClicked: StoreEntityId is 0"));
			}
			
			NGIS->SendStoreUse(StoreEntityId, static_cast<uint32>(ItemID));
		}
	}
}

void UStoreSlotWidget::PlayPurchaseSuccessSound() const
{
	if (!PurchaseSuccessSound)
	{
		return;
	}

	UGameplayStatics::PlaySound2D(this, PurchaseSuccessSound);
}

void UStoreSlotWidget::OnStorePriceDataUpdated()
{
	UpdateUI();
}

void UStoreSlotWidget::OnStorePurchaseSucceeded(uint32 PurchasedItemID, int32 NewPrice, bool bIsSoldOut)
{
	if (PurchasedItemID == static_cast<uint32>(ItemID))
	{
		if (Price_Text && NewPrice > 0)
		{
			Price_Text->SetText(FText::AsNumber(NewPrice));
		}
		else if (Price_Text && bIsSoldOut)
		{
			Price_Text->SetText(SoldOutPriceText);
		}
		if (Slot_Button)
		{
			Slot_Button->SetIsEnabled(!bIsSoldOut);
		}
		PlayPurchaseSuccessSound();
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
			bool bUseServerPrice = false;
			bool bIsSoldOut = false;
			if (UNetworkGameInstanceSubsystem* NGIS = UNetworkGameInstanceSubsystem::Get(this))
			{
				bUseServerPrice = NGIS->GetStoreItemPrice(static_cast<uint32>(ItemID), Price);
				bIsSoldOut = NGIS->IsStoreItemSoldOut(static_cast<uint32>(ItemID));
			}

			if (!bUseServerPrice)
			{
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
			}
			Price_Text->SetText(bIsSoldOut ? SoldOutPriceText : FText::AsNumber(Price));
			if (Slot_Button)
			{
				Slot_Button->SetIsEnabled(!bIsSoldOut);
			}
		}
	}
}
