// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventoryWidget.generated.h"


class UVerticalBox;
class UListView;
class ATimeThiefPlayerCharacter;
/**
 * 
 */
UCLASS()
class TIMETHIEF_API UInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UListView> VicinityItem_ListView;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UListView> Inventory_ListView;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> Vicinity_VerticalBox;
	
	UPROPERTY()
	TWeakObjectPtr<ATimeThiefPlayerCharacter> Player;

protected:
	void OnVicinityItemUpdated();
	
	void OnInventoryItemUpdated();
	
public:
	virtual void SetVisibility(ESlateVisibility InVisibility) override;
	
	UFUNCTION(BlueprintCallable)
	void Init(ATimeThiefPlayerCharacter* InPlayer);
};
