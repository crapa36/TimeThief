// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Item/ItemSlotWidgetBase.h"
#include "ItemCommons.h"
#include "StoreSlotWidget.generated.h"

class UTextBlock;
class UImage;
class UButton;
/**
 * 
 */
UCLASS()
class TIMETHIEF_API UStoreSlotWidget : public UItemSlotWidgetBase
{
	GENERATED_BODY()
	
protected:
	virtual void NativeConstruct() override;
	
	UPROPERTY(meta =(BindWidget))
	TObjectPtr<UTextBlock> Price_Text;
	
public:
	virtual void OnSlotClicked() override;
	
	void Init(EItemID InItemID);
	
	virtual void UpdateUI() override;
};

