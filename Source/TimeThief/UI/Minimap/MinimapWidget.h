// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MinimapWidget.generated.h"

/**
 * 
 */

class ATimeThiefGameState;
class AStoreActor;
class UCanvasPanel;
class UImage;
class UMaterialInstanceDynamic;
class UTexture2D;

UCLASS()
class TIMETHIEF_API UMinimapWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	virtual void NativeConstruct() override;

	virtual void NativeDestruct() override;
	
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
	TObjectPtr<UMaterialInstanceDynamic> StormZoneMID;
	
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> NextStormZoneMID;
	
	UPROPERTY(EditAnywhere)
	TObjectPtr<AActor> Player;
	
	UPROPERTY()
	TObjectPtr<ATimeThiefGameState> GameState;

	FVector2D MinimapSize;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "TimeThief|Minimap")
	TObjectPtr<UTexture2D> StoreIconTexture;

	UPROPERTY(EditDefaultsOnly, Category = "TimeThief|Minimap")
	FVector2D StoreIconSize = FVector2D(40.0f, 40.0f);

private:
	FVector2D WorldToMinimapPosition(const FVector& WorldLocation, const FVector2f& MapSize) const;
	void GatherStoreActors(UWorld* World, TArray<AStoreActor*>& OutStoreActors) const;
	void UpdateStoreIcons(UWorld* World, const FVector2f& MapSize);
	UImage* GetOrCreateStoreIcon(AStoreActor* StoreActor);
	void RemoveStaleStoreIcons(const TSet<TWeakObjectPtr<AStoreActor>>& ActiveStores);
	void ClearStoreIcons();

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> MinimapCanvas;

	TMap<TWeakObjectPtr<AStoreActor>, TObjectPtr<UImage>> StoreIcons;
};
