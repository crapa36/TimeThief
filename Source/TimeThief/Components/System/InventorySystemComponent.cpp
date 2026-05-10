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
	
	if (UGameItemData* LoadedData = GetDefault<UItemSettings>()->GetItemData())
	{
		for (EItemID ItemID : TEnumRange<EItemID>())
		{
			if (LoadedData->Items[ItemID].Category == EItemCategory::Consumable)
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
	
	if (Index < 0 || Index >= ItemQuantities.Num())
	{
		return;
	}
	
	if (UGameItemData* LoadedData = GetDefault<UItemSettings>()->GetItemData())
	{
		if (ConsumableEquipment == EItemID::SIZE)
		{
			if (LoadedData->Items[ItemID].Category == EItemCategory::Consumable)
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

			if (UGameItemData* LoadedData = GetDefault<UItemSettings>()->GetItemData())
			{
				if (ConsumableEquipment == ItemID)
				{
					if (LoadedData->Items[ItemID].Category == EItemCategory::Consumable)
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

void UInventorySystemComponent::SetInventory(const TArray<TPair<EItemID,int>>& NewInventory)
{
	if (ItemQuantities.Num() != NewInventory.Num())
	{
		return;
	}
	
	for (const auto& [ItemID, Quantity] : NewInventory)
	{
		int Index = static_cast<int>(ItemID) - static_cast<int>(EItemID::SmallPotion);
		if (Index < 0 || Index >= ItemQuantities.Num())
		{
			continue;
		}
		
		ItemQuantities[Index]->Quantity = Quantity;
	}
	OnInventoryUpdatedEvent.Broadcast();
}

void UInventorySystemComponent::SetEquipment(EItemID ItemID)
{
	if (UGameItemData* LoadedData = GetDefault<UItemSettings>()->GetItemData())
	{
		if (LoadedData->Items[ItemID].Category == EItemCategory::Consumable)
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
