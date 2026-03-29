// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ItemCommons.h"
#include "Blueprint/UserWidget.h"
#include "EquipmentWidget.generated.h"

/**
 * 
 */

class UImage;
class UPromptWidget;

UCLASS()
class TIMETHIEF_API UEquipmentWidget : public UUserWidget
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, meta = (BindWidget))
	TObjectPtr<UPromptWidget> PromptWidget;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> ItemIcon_Image;
	
public:
	virtual void NativeConstruct() override;
	
	virtual void NativeDestruct() override;
	
	UPROPERTY(EditAnywhere, Category="Equipment Type")
	EItemCategory EquipmentCategory;
	
private:
	void OnChangedEquipment(EItemID InItemID);
};
