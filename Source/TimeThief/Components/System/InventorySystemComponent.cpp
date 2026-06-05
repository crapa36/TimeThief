// Fill out your copyright notice in the Description page of Project Settings.


#include "InventorySystemComponent.h"

#include "DataAssets/GameItemData.h"
#include "Game/ItemSettings.h"
#include "Kismet/KismetSystemLibrary.h"

namespace
{
	bool IsKnownThrowableItem(EItemID ItemID)
	{
		return ItemID == EItemID::Grenade || ItemID == EItemID::SmokeGrenade;
	}

	bool IsTrackedInventoryItem(const FItemData* ItemData, EItemID ItemID)
	{
		return IsKnownThrowableItem(ItemID)
			|| (ItemData && (ItemData->Category == EItemCategory::Consumable || ItemData->Category == EItemCategory::Throwable));
	}

	bool IsConsumableItem(const FItemData* ItemData)
	{
		return ItemData && ItemData->Category == EItemCategory::Consumable;
	}

	bool IsThrowableItem(const FItemData* ItemData, EItemID ItemID)
	{
		return IsKnownThrowableItem(ItemID) || (ItemData && ItemData->Category == EItemCategory::Throwable);
	}
}


// Sets default values for this component's properties
UInventorySystemComponent::UInventorySystemComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
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

	InitializeInventoryObjects();
}

void UInventorySystemComponent::InitializeInventoryObjects()
{
	ItemQuantities.Reset();
	ConsumableEquipment = EItemID::SIZE;
	ThrowableEquipment = EItemID::SIZE;

	if (UGameItemData* LoadedData = GetDefault<UItemSettings>()->GetItemData())
	{
		for (EItemID ItemID : TEnumRange<EItemID>())
		{
			const FItemData* ItemData = LoadedData->Items.Find(ItemID);
			if (!IsTrackedInventoryItem(ItemData, ItemID))
			{
				continue;
			}

			ItemQuantities.Add(NewObject<UInventoryObject>(this, UInventoryObject::StaticClass()));
			ItemQuantities.Last()->ItemID = ItemID;
		}
	}
}

void UInventorySystemComponent::ClearInventory()
{
	if (ItemQuantities.IsEmpty())
	{
		InitializeInventoryObjects();
	}

	for (UInventoryObject* InventoryObject : ItemQuantities)
	{
		if (InventoryObject && InventoryObject->Quantity != 0)
		{
			InventoryObject->Quantity = 0;
			InventoryObject->OnInventoryObjectUpdatedEvent.Broadcast();
		}
	}

	if (ConsumableEquipment != EItemID::SIZE)
	{
		SetConsumableEquipment(EItemID::SIZE);
	}
	if (ThrowableEquipment != EItemID::SIZE)
	{
		SetThrowableEquipment(EItemID::SIZE);
	}

	OnInventoryUpdatedEvent.Broadcast();
}

void UInventorySystemComponent::AddItem(EItemID ItemID, int Amount)
{
	if (Amount <= 0)
	{
		return;
	}

	UInventoryObject* InventoryObject = FindInventoryObject(ItemID);
	if (!InventoryObject)
	{
		return;
	}
	
	if (UGameItemData* LoadedData = GetDefault<UItemSettings>()->GetItemData())
	{
		const FItemData* ItemData = LoadedData->Items.Find(ItemID);
		if (!IsTrackedInventoryItem(ItemData, ItemID))
		{
			return;
		}

		if (ConsumableEquipment == EItemID::SIZE)
		{
			if (IsConsumableItem(ItemData))
			{
				SetConsumableEquipment(ItemID);
			}
		}
		if (ThrowableEquipment == EItemID::SIZE)
		{
			if (IsThrowableItem(ItemData, ItemID))
			{
				SetThrowableEquipment(ItemID);
			}
		}
	}

	int PrevQuantity = InventoryObject->Quantity;

	InventoryObject->Quantity += Amount;
	InventoryObject->OnInventoryObjectUpdatedEvent.Broadcast();

	if (PrevQuantity == 0)
	{
		OnInventoryUpdatedEvent.Broadcast();
	}
}

bool UInventorySystemComponent::RemoveItem(EItemID ItemID, int Amount)
{
	if (Amount <= 0)
	{
		return false;
	}

	UInventoryObject* InventoryObject = FindInventoryObject(ItemID);
	if (!InventoryObject)
	{
		return false;
	}

	if (InventoryObject->Quantity >= Amount)
	{
		InventoryObject->Quantity -= Amount;
		InventoryObject->OnInventoryObjectUpdatedEvent.Broadcast();
		if (InventoryObject->Quantity == 0)
		{
			OnInventoryUpdatedEvent.Broadcast();

			if (UGameItemData* LoadedData = GetDefault<UItemSettings>()->GetItemData())
			{
				const FItemData* ItemData = LoadedData->Items.Find(ItemID);
				if (!IsTrackedInventoryItem(ItemData, ItemID))
				{
					return true;
				}

				if (ConsumableEquipment == ItemID)
				{
					if (IsConsumableItem(ItemData))
					{
						SetConsumableEquipment(EItemID::SIZE);
					}
				}
				if (ThrowableEquipment == ItemID)
				{
					if (IsThrowableItem(ItemData, ItemID))
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

void UInventorySystemComponent::SetInventory(const TArray<TPair<EItemID,int>>& NewInventory)
{
	for (const auto& [ItemID, Quantity] : NewInventory)
	{
		if (UInventoryObject* InventoryObject = FindInventoryObject(ItemID))
		{
			InventoryObject->Quantity = FMath::Max(0, Quantity);
			InventoryObject->OnInventoryObjectUpdatedEvent.Broadcast();
		}
	}
	OnInventoryUpdatedEvent.Broadcast();
}

void UInventorySystemComponent::SetEquipment(EItemID ItemID)
{
	if (ItemID == EItemID::SIZE)
	{
		return;
	}

	if (GetItemQuantity(ItemID) <= 0)
	{
		return;
	}

	if (UGameItemData* LoadedData = GetDefault<UItemSettings>()->GetItemData())
	{
		const FItemData* ItemData = LoadedData->Items.Find(ItemID);
		if (!IsTrackedInventoryItem(ItemData, ItemID))
		{
			return;
		}

		if (IsConsumableItem(ItemData))
		{
			SetConsumableEquipment(ItemID);
		}
		else if (IsThrowableItem(ItemData, ItemID))
		{
			SetThrowableEquipment(ItemID);
		}
	}
}

UInventoryObject* UInventorySystemComponent::FindInventoryObject(EItemID ItemID) const
{
	for (UInventoryObject* InventoryObject : ItemQuantities)
	{
		if (InventoryObject && InventoryObject->ItemID == ItemID)
		{
			return InventoryObject;
		}
	}

	return nullptr;
}

int UInventorySystemComponent::GetItemQuantity(EItemID ItemID) const
{
	const UInventoryObject* InventoryObject = FindInventoryObject(ItemID);
	return InventoryObject ? InventoryObject->Quantity : 0;
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
