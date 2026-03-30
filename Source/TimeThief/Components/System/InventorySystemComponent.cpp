// Fill out your copyright notice in the Description page of Project Settings.


#include "InventorySystemComponent.h"

#include "DataAssets/GameItemData.h"
#include "Game/ItemSettings.h"
#include "Kismet/KismetSystemLibrary.h"


// Sets default values for this component's properties
UInventorySystemComponent::UInventorySystemComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
}


// Called when the game starts
void UInventorySystemComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
}

void UInventorySystemComponent::OnRegister()
{
	Super::OnRegister();

	const UItemSettings* StoreSettings = GetDefault<UItemSettings>();
	if (UGameItemData* LoadedData = StoreSettings->ItemData.LoadSynchronous())
	{
		for (EItemID ItemID : TEnumRange<EItemID>())
		{
			if (LoadedData->Items[ItemID].Category == EItemCategory::Consumable && ItemID != EItemID::TimePoint)
			{
				ItemQuantities.Add(NewObject<UInventoryObject>(this, UInventoryObject::StaticClass()));
				ItemQuantities.Last()->ItemID = ItemID;
			}
			else if (LoadedData->Items[ItemID].Category == EItemCategory::Throwable)
			{
				ItemQuantities.Add(NewObject<UInventoryObject>(this, UInventoryObject::StaticClass()));
				ItemQuantities.Last()->ItemID = ItemID;
			}
		}
	}
}


// Called every frame
void UInventorySystemComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                              FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UInventorySystemComponent::AddItem(EItemID ItemID, int Amount)
{
	int Index = static_cast<int>(ItemID) - static_cast<int>(EItemID::SmallPotion);

	UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Index %d"), Index));


	if (Index < 0 || Index >= ItemQuantities.Num())
	{
		return;
	}

	const UItemSettings* StoreSettings = GetDefault<UItemSettings>();
	if (UGameItemData* LoadedData = StoreSettings->ItemData.LoadSynchronous())
	{
		if (ConsumableEquipment == EItemID::SIZE)
		{
			if (LoadedData->Items[ItemID].Category == EItemCategory::Consumable && ItemID != EItemID::TimePoint)
			{
				SetConsumableEquipment(ItemID);
			}
		}
		if (ThrowableEquipment == EItemID::SIZE)
		{
			if (LoadedData->Items[ItemID].Category == EItemCategory::Throwable)
			{
				SetThrowableEquipment(ItemID);
			}
		}
	}

	int PrevQuantity = ItemQuantities[Index]->Quantity;

	ItemQuantities[Index]->Quantity += Amount;
	ItemQuantities[Index]->OnInventoryObjectUpdatedEvent.Broadcast();

	if (PrevQuantity == 0)
	{
		OnInventoryUpdatedEvent.Broadcast();
	}
}

bool UInventorySystemComponent::RemoveItem(EItemID ItemID, int Amount)
{
	int Index = static_cast<int>(ItemID) - static_cast<int>(EItemID::SmallPotion);
	if (Index < 0 || Index >= ItemQuantities.Num())
	{
		return false;
	}

	if (ItemQuantities[Index]->Quantity >= Amount)
	{
		ItemQuantities[Index]->Quantity -= Amount;
		ItemQuantities[Index]->OnInventoryObjectUpdatedEvent.Broadcast();
		if (ItemQuantities[Index]->Quantity == 0)
		{
			OnInventoryUpdatedEvent.Broadcast();

			const UItemSettings* StoreSettings = GetDefault<UItemSettings>();
			if (UGameItemData* LoadedData = StoreSettings->ItemData.LoadSynchronous())
			{
				if (ConsumableEquipment == ItemID)
				{
					if (LoadedData->Items[ItemID].Category == EItemCategory::Consumable && ItemID != EItemID::TimePoint)
					{
						SetConsumableEquipment(EItemID::SIZE);
					}
				}
				if (ThrowableEquipment == ItemID)
				{
					if (LoadedData->Items[ItemID].Category == EItemCategory::Throwable)
					{
						SetThrowableEquipment(EItemID::SIZE);
					}
				}
			}
		}
		return true;
	}

	return false;
}

void UInventorySystemComponent::SetEquipment(EItemID ItemID)
{
	const UItemSettings* StoreSettings = GetDefault<UItemSettings>();
	if (UGameItemData* LoadedData = StoreSettings->ItemData.LoadSynchronous())
	{
		if (LoadedData->Items[ItemID].Category == EItemCategory::Consumable && ItemID != EItemID::TimePoint)
		{
			SetConsumableEquipment(ItemID);
		}
		else if (LoadedData->Items[ItemID].Category == EItemCategory::Throwable)
		{
			SetThrowableEquipment(ItemID);
		}
	}
}

void UInventorySystemComponent::SetConsumableEquipment(EItemID ItemID)
{
	ConsumableEquipment = ItemID;
	OnConsumableEquipmentUpdatedEvent.Broadcast(ConsumableEquipment);
}

void UInventorySystemComponent::SetThrowableEquipment(EItemID ItemID)
{
	ThrowableEquipment = ItemID;
	OnThrowableEquipmentUpdatedEvent.Broadcast(ThrowableEquipment);
}
