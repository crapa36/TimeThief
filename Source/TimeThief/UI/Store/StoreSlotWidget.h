// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "StoreCommons.h"
#include "StoreSlotWidget.generated.h"

class UTextBlock;
class UImage;
class UButton;
/**
 * 
 */
UCLASS()
class TIMETHIEF_API UStoreSlotWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	virtual void NativeConstruct() override;
	
	UPROPERTY(meta =(BindWidget))
	TObjectPtr<UButton> Slot_Button;
	
	UPROPERTY(meta =(BindWidget))
	TObjectPtr<UImage> Item_Image;
	
	UPROPERTY(meta =(BindWidget))
	TObjectPtr<UTextBlock> Price_Text;
	
	UPROPERTY(meta =(BindWidget))
	TObjectPtr<UTextBlock> Item_Text;
	
	EStoreItemName ItemName;
	
public:
	UFUNCTION()
	void OnSlotClicked();
	
	void Init(EStoreItemName InItemName);
	
	void UpdateUI();
};

