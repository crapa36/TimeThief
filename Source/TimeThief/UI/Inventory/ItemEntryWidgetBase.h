// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "UI/Item/ItemSlotWidgetBase.h"
#include "ItemEntryWidgetBase.generated.h"


class UBorder;
class AItemBase;
/**
 * 
 */
UCLASS()
class TIMETHIEF_API UItemEntryWidgetBase : public UItemSlotWidgetBase, public IUserObjectListEntry
{
	GENERATED_BODY()

protected:
	UPROPERTY(meta =(BindWidget))
	TObjectPtr<UTextBlock> ItemQuantity_Text;

	UPROPERTY(meta =(BindWidget))
	TObjectPtr<UBorder> ItemBackground_Border;

public:
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	
protected:
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

private:
	UPROPERTY(EditAnywhere, Category = "UI")
	FLinearColor OnMouseColor{0.01, 0.01, 0.01, 1};
	
	UPROPERTY(EditAnywhere, Category = "UI")
	FLinearColor OriginColor{0.5, 0.5, 0.5, 0.5};
};
