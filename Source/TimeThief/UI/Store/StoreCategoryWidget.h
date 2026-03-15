// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "StoreCommons.h"
#include "Blueprint/UserWidget.h"
#include "StoreCategoryWidget.generated.h"

class UTextBlock;
class UStoreCategoryData;
class UStoreItemData;
class UStoreSlotWidget;
class UHorizontalBox;
/**
 * 
 */
UCLASS()
class TIMETHIEF_API UStoreCategoryWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	virtual void NativePreConstruct() override;
	
	UPROPERTY(EditAnywhere, Category = "Store")
	EStoreItemType ItemType;
	
	UPROPERTY(EditDefaultsOnly, Category = "Store")
	TSubclassOf<UStoreSlotWidget> SlotClass;
	
	UPROPERTY(meta =(BindWidget))
	TObjectPtr<UHorizontalBox> Items_HorizontalBox;
	
	UPROPERTY(meta =(BindWidget))
	TObjectPtr<UTextBlock> Category_Text;
};
