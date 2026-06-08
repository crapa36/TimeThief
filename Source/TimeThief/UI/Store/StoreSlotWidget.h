// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Item/ItemSlotWidgetBase.h"
#include "ItemCommons.h"
#include "StoreSlotWidget.generated.h"

class UTextBlock;
class UImage;
class UButton;
class USoundBase;
/**
 * 
 */
UCLASS()
class TIMETHIEF_API UStoreSlotWidget : public UItemSlotWidgetBase
{
	GENERATED_BODY()
	
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	
	UPROPERTY(meta =(BindWidget))
	TObjectPtr<UTextBlock> Price_Text;

	UPROPERTY(EditDefaultsOnly, Category = "Store|Feedback")
	TObjectPtr<USoundBase> PurchaseSuccessSound = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Store|Display")
	FText SoldOutPriceText = FText::FromString(TEXT("None"));
	
public:
	virtual void OnSlotClicked() override;
	
	void Init(EItemID InItemID);
	
	virtual void UpdateUI() override;

private:
	void PlayPurchaseSuccessSound() const;
	void OnStorePriceDataUpdated();
	void OnStorePurchaseSucceeded(uint32 PurchasedItemID, int32 NewPrice, bool bIsSoldOut);
};

