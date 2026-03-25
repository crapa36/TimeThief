// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ItemCommons.h"
#include "ItemToolTipWidget.generated.h"

/**
 * 
 */

class UImage;
class UTextBlock;

UCLASS()
class TIMETHIEF_API UItemToolTipWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	UPROPERTY(meta =(BindWidget))
	TObjectPtr<UTextBlock> ItemName_Text;
	
	UPROPERTY(meta =(BindWidget))
	TObjectPtr<UTextBlock> CategoryName_Text;
	
	UPROPERTY(meta =(BindWidget))
	TObjectPtr<UImage> Item_Image;
	
	UPROPERTY(meta =(BindWidget))
	TObjectPtr<UTextBlock> ItemDesc_Text;
	
	UPROPERTY(meta =(BindWidget))
	TObjectPtr<UTextBlock> Stat_Text;
	
	void SetupToolTip(EItemID InItemID);
};
