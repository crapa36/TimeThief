// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ItemCommons.h"
#include "Actors/Item/ItemBase.h"
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
	EItemCategory Category = EItemCategory::Consumable;
	
	UPROPERTY(EditAnywhere)
	int Price = 1;
	
	UPROPERTY(EditAnywhere)
	int Increment = 0;
	
	UPROPERTY(EditAnywhere)
	TSubclassOf<AActor> ItemClass = AItemBase::StaticClass();
	
	UPROPERTY(EditAnywhere)
	TSoftObjectPtr<UStaticMesh> ItemMesh = nullptr;
};

UCLASS()
class TIMETHIEF_API UGameItemData : public UDataAsset
{
	GENERATED_BODY()
	
public:
	UGameItemData();
	
	UPROPERTY(EditAnywhere)
	TMap<EItemID, FItemData> Items;
	
	TSoftObjectPtr<UStaticMesh> GetItemMeshSoft(EItemID ItemID) const
	{
		if (const TSoftObjectPtr<UStaticMesh> MeshPtr = Items.Find(ItemID)->ItemMesh)
		{
			return MeshPtr;
		}
		return nullptr;
	}
	
	UStaticMesh* GetItemMesh(EItemID ItemID) const
	{
		auto MeshPtr = GetItemMeshSoft(ItemID);
		if (MeshPtr.IsNull())
		{
			return nullptr;
		}
		
		return MeshPtr.LoadSynchronous();
	}
	
	TSubclassOf<AActor> GetItemClass(EItemID ItemID) const
	{
		if (Items.Contains(ItemID))
		{
			return Items[ItemID].ItemClass;
		}
		
		return nullptr;
	}
	
	TSubclassOf<AActor> GetItemClass(uint32 ItemID) const
	{
		return GetItemClass(static_cast<EItemID>(ItemID));
	}
};
