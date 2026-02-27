// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MinimapWidget.generated.h"

/**
 * 
 */

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
	
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> StormZoneDMI;
	
	UPROPERTY(EditAnywhere)
	TObjectPtr<AActor> Player;
	
	UPROPERTY(EditAnywhere)
	FVector2D MapSize;
	
	FVector2D MinimapSize;
	
	float ElapsedTime{0};
	FVector2D CurrentStormZoneCenter{0.5f};
	float CurrentStormZoneRadius{0.5f};
	
	FVector2D StormZoneCenter{0.5f};
	float StormZoneRadius{0.5f};
	
	FVector2D DestStormZoneCenter{0.7f, 0.7f};
	float DestStormZoneRadius{0.3f};
};
