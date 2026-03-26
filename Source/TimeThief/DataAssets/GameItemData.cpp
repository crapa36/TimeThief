// Fill out your copyright notice in the Description page of Project Settings.


#include "GameItemData.h"

UGameItemData::UGameItemData()
{
	for (EItemID ItemName : TEnumRange<EItemID>())
	{
		StoreItems.Add(ItemName, FItemData{});
	}
}
