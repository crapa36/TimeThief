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
struct FStoreItemInfo
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere)
	FString Name = "None";
	
	UPROPERTY(EditAnywhere)
	TObjectPtr<UTexture2D> Icon = nullptr;
	
	UPROPERTY(EditAnywhere)
	int Price = 0;
	
	UPROPERTY(EditAnywhere)
	int Increment = 0;
};

UCLASS()
class TIMETHIEF_API UStoreItemData : public UDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere)
	TMap<EStoreItemName, FStoreItemInfo> StoreItems;
};
