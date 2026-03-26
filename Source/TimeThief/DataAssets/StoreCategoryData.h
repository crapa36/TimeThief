// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ItemCommons.h"
#include "StoreCategoryData.generated.h"

/**
 * 
 */
USTRUCT(BlueprintType)
struct FStoreItemList
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FString CategoryName;
	
	UPROPERTY(EditAnywhere)
	TArray<EItemID> ItemList;
};

UCLASS()
class TIMETHIEF_API UStoreCategoryData : public UDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere)
	TMap<EItemCategory, FStoreItemList> StoreItemMap;
};
