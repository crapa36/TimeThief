// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "DataAssets/StoreItemData.h"
#include "DataAssets/StoreCategoryData.h"
#include "StoreSettings.generated.h"

/**
 * 
 */
UCLASS(config = Game, defaultconfig, meta=(DisplayName="Store Settings"))
class TIMETHIEF_API UStoreSettings : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:
	UPROPERTY(config, EditAnywhere, Category = "Store")
	TSoftObjectPtr<UStoreCategoryData> CategoryData;
	
	UPROPERTY(config, EditAnywhere, Category = "Store")
	TSoftObjectPtr<UStoreItemData> ItemData;
};
