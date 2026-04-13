// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ItemWheelSlotWidget.generated.h"

class UImage;
class UTextBlock;
/**
 * 
 */
UCLASS()
class TIMETHIEF_API UItemWheelSlotWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void SetData(const FString& Name, UTexture2D* Icon);

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> ItemImage;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ItemText;
};
