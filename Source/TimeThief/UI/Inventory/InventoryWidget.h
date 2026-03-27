// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventoryWidget.generated.h"


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
	
	UPROPERTY()
	TWeakObjectPtr<ATimeThiefPlayerCharacter> Player;

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

public:
	UFUNCTION(BlueprintCallable)
	void Init(ATimeThiefPlayerCharacter* InPlayer);
};
