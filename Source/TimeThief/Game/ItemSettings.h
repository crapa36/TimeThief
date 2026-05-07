// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "IDetailTreeNode.h"
#include "Engine/DeveloperSettings.h"
#include "DataAssets/GameItemData.h"
#include "DataAssets/StoreCategoryData.h"
#include "DataAssets/UpgradeData.h"
#include "ItemSettings.generated.h"

/**
 * 
 */
UCLASS(config = Game, defaultconfig, meta=(DisplayName="Item Settings"))
class TIMETHIEF_API UItemSettings : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:
	UPROPERTY(config, EditAnywhere, Category = "Item")
	TSoftObjectPtr<UGameItemData> ItemData;
	
	UPROPERTY(config, EditAnywhere, Category = "Store")
	TSoftObjectPtr<UStoreCategoryData> CategoryData;

	UPROPERTY(config, EditAnywhere, Category = "Store")
	TSoftObjectPtr<UUpgradeData> UpgradeData;
	
	UGameItemData* GetItemData() const
	{
		return ItemData.LoadSynchronous();
	}
	
	TSoftObjectPtr<UStaticMesh> GetItemMeshSoft(EItemID ItemID) const
	{
		return ItemData.LoadSynchronous()->GetItemMeshSoft(ItemID);
	}
	
	UStaticMesh* GetItemMesh(EItemID ItemID) const
	{
		return ItemData.LoadSynchronous()->GetItemMesh(ItemID);
	}
	
	TSubclassOf<AActor> GetItemClass(EItemID ItemID) const
	{
		return ItemData.LoadSynchronous()->GetItemClass(ItemID);
	}
	
	TSubclassOf<AActor> GetItemClass(uint32 ItemID) const
	{
		return GetItemClass(static_cast<EItemID>(ItemID));
	}
	
	const FString& GetItemName(EItemID ItemID) const
	{
		static const FString EmptyString;
		
		const auto& Items = GetItemData()->Items;
		
		if (Items.Contains(ItemID))
		{
			return Items[ItemID].Name;
		}
		
		return EmptyString;
	}
};
