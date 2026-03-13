// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "StoreCommons.h"
#include "StoreItemData.generated.h"

/**
 * 
 */
USTRUCT(BlueprintType)
struct FStoreItemStat
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere)
	FString Name;
	
	UPROPERTY(EditAnywhere)
	TObjectPtr<UTexture2D> Icon;
	
	UPROPERTY(EditAnywhere)
	int Price;
	
	UPROPERTY(EditAnywhere)
	int Increment;
};

UCLASS()
class TIMETHIEF_API UStoreItemData : public UDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere)
	TMap<EStoreItemName, FStoreItemStat> StoreItems;
};
