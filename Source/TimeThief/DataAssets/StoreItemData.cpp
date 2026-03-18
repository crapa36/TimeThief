// Fill out your copyright notice in the Description page of Project Settings.


#include "StoreItemData.h"

UStoreItemData::UStoreItemData()
{
	for (EStoreItemName ItemName : TEnumRange<EStoreItemName>())
	{
		StoreItems.Add(ItemName, FStoreItemInfo{});
	}
}
