// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ItemToolTipWidget.generated.h"

/**
 * 
 */

class UTextBlock;

struct FItemToolTipData
{
	FString Name;
	FString Description;
};

UCLASS()
class TIMETHIEF_API UItemToolTipWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	UPROPERTY(meta =(BindWidget))
	TObjectPtr<UTextBlock> ItemName_Text;
	
	UPROPERTY(meta =(BindWidget))
	TObjectPtr<UTextBlock> ItemDesc_Text;
	
	void SetupTooltip(const FItemToolTipData& Tooltip);
};
