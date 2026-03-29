// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ItemCommons.h"
#include "Engine/DataAsset.h"
#include "GameItemData.generated.h"

/**
 * 
 */
USTRUCT(BlueprintType)
struct FItemData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere)
	FString Name = "None";
	
	UPROPERTY(EditAnywhere)
	FString Description = "None";
	
	UPROPERTY(EditAnywhere)
	FString Stat = "";
	
	UPROPERTY(EditAnywhere)
	TObjectPtr<UTexture2D> Icon = nullptr;
	
	UPROPERTY(EditAnywhere)
	EItemCategory Category;
	
	UPROPERTY(EditAnywhere)
	int Price = 0;
	
	UPROPERTY(EditAnywhere)
	int Increment = 0;
};

UCLASS()
class TIMETHIEF_API UGameItemData : public UDataAsset
{
	GENERATED_BODY()
	
public:
	UGameItemData();
	
	UPROPERTY(EditAnywhere)
	TMap<EItemID, FItemData> Items;
};
