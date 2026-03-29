// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ItemEntryWidgetBase.h"
#include "VicinityItemEntryWidget.generated.h"

/**
 * 
 */
UCLASS()
class TIMETHIEF_API UVicinityItemEntryWidget : public UItemEntryWidgetBase
{
	GENERATED_BODY()
	
	UPROPERTY(meta =(BindWidget))
	TObjectPtr<UTextBlock> Interaction_Text;
	
public:
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

	virtual void UpdateUI() override;
	
protected:
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	
	virtual void OnSlotClicked() override;
	
private:
	void BindItem(AItemBase* InItemActor);
	
	UPROPERTY()
	TWeakObjectPtr<AItemBase> BindedItem;
};
