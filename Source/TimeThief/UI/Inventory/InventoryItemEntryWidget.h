// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ItemEntryWidgetBase.h"
#include "InventoryItemEntryWidget.generated.h"

/**
 * 
 */

class UInventoryObject;

UCLASS()
class TIMETHIEF_API UInventoryItemEntryWidget : public UItemEntryWidgetBase
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<UInventoryObject> Item;

public:
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

	virtual void UpdateUI() override;

protected:
	virtual void OnSlotClicked() override;

private:
	void OnInventoryObjectUpdated();

	void BindItem(UInventoryObject* InItem);
};
