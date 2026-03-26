// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "DataAssets/GameItemData.h"
#include "DataAssets/StoreCategoryData.h"
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
};
