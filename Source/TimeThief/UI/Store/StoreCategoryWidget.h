// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ItemCommons.h"
#include "Blueprint/UserWidget.h"
#include "StoreCategoryWidget.generated.h"

class UTextBlock;
class UStoreCategoryData;
class UGameItemData;
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
	EItemCategory ItemType;
	
	UPROPERTY(EditDefaultsOnly, Category = "Store")
	TSubclassOf<UStoreSlotWidget> SlotClass;
	
	UPROPERTY(meta =(BindWidget))
	TObjectPtr<UHorizontalBox> Items_HorizontalBox;
	
	UPROPERTY(meta =(BindWidget))
	TObjectPtr<UTextBlock> Category_Text;
};
