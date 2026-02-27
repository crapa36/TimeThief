// Fill out your copyright notice in the Description page of Project Settings.


#include "MinimapWidget.h"

#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"

void UMinimapWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	Player = GetOwningPlayerPawn();
	MinimapSize = Cast<UCanvasPanelSlot>(Minimap_Image->Slot)->GetSize();
	
	StormZoneDMI = StormZone_Image->GetDynamicMaterial();
}

void UMinimapWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	
	if (IsVisible() && IsValid(Player))
	{
		const FVector PlayerLocation = Player->GetActorLocation();
		FVector2D MinimapPosition;
		
		MinimapPosition.X = PlayerLocation.Y / MapSize.Y * MinimapSize.X;
		MinimapPosition.Y = PlayerLocation.X / MapSize.X * -MinimapSize.Y;
		
		Player_Icon->SetRenderTranslation(MinimapPosition);
	}
	ElapsedTime += InDeltaTime;
	if (StormZoneCenter.Equals(DestStormZoneCenter))
	{
		
	}
	else
	{
		float AlphaRatio = FMath::Min(ElapsedTime / 10, 1);
		StormZoneCenter = FMath::Lerp(CurrentStormZoneCenter, DestStormZoneCenter, AlphaRatio);
		StormZoneRadius = FMath::Lerp(CurrentStormZoneRadius, DestStormZoneRadius, AlphaRatio);
		StormZoneDMI->SetVectorParameterValue(FName{"CenterPosition"}, FVector{StormZoneCenter.X, StormZoneCenter.Y, 0});
		StormZoneDMI->SetScalarParameterValue(FName{"Radius"}, StormZoneRadius);
	}
}
