// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MinimapWidget.generated.h"

/**
 * 
 */

class ATimeThiefGameState;
class UImage;
class UMaterialInstanceDynamic;

UCLASS()
class TIMETHIEF_API UMinimapWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	virtual void NativeConstruct() override;
	
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	
public:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Minimap_Image;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Player_Icon;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> StormZone_Image;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> NextStormZone_Image;
	
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> StormZoneDMI;
	
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> NextStormZoneDMI;
	
	UPROPERTY(EditAnywhere)
	TObjectPtr<AActor> Player;
	
	UPROPERTY()
	TObjectPtr<ATimeThiefGameState> GameState;

	FVector2D MinimapSize;
};
