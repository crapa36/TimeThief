// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "UI/Item/ItemSlotWidgetBase.h"
#include "VicinityItemSlotWidget.generated.h"


class UBorder;
class AItemBase;
/**
 * 
 */
UCLASS()
class TIMETHIEF_API UVicinityItemSlotWidget : public UItemSlotWidgetBase, public IUserObjectListEntry
{
	GENERATED_BODY()

	UPROPERTY(meta =(BindWidget))
	TObjectPtr<UTextBlock> ItemQuantity_Text;

	UPROPERTY(meta =(BindWidget))
	TObjectPtr<UTextBlock> Interaction_Text;

	UPROPERTY(meta =(BindWidget))
	TObjectPtr<UBorder> ItemBackground_Border;

	UPROPERTY()
	TWeakObjectPtr<AItemBase> BindedItem;
	
	UPROPERTY(EditAnywhere, Category = "UI")
	FLinearColor OnMouseColor{0.01, 0.01, 0.01, 1};
	
	UPROPERTY(EditAnywhere, Category = "UI")
	FLinearColor OriginColor{0.5, 0.5, 0.5, 0.5};
	
protected:
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

public:
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

	virtual void UpdateUI() override;

protected:
	virtual void OnSlotClicked() override;

private:
	void BindItem(AItemBase* InItemActor);
};
