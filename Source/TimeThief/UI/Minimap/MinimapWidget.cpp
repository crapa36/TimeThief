// Fill out your copyright notice in the Description page of Project Settings.


#include "MinimapWidget.h"

#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/System/TimeStormComponent.h"
#include "Game/TimeThiefGameState.h"

void UMinimapWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	MinimapSize = Cast<UCanvasPanelSlot>(Minimap_Image->Slot)->GetSize();

	StormZoneMID = StormZone_Image->GetDynamicMaterial();
	NextStormZoneMID = NextStormZone_Image->GetDynamicMaterial();
	
	GameState = GetWorld()->GetGameState<ATimeThiefGameState>();
}

void UMinimapWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const FVector2f& MapSize = GameState->TimeStormComponent->MapSize;
	
	if (IsVisible())
	{
		const FVector PlayerLocation = GetWorld()->GetFirstPlayerController()->GetPawn()->GetActorLocation();
		FVector2D MinimapPosition;
		MinimapPosition.X = PlayerLocation.Y / MapSize.Y * MinimapSize.X;
		MinimapPosition.Y = PlayerLocation.X / MapSize.X * -MinimapSize.Y;

		Player_Icon->SetRenderTranslation(MinimapPosition);
		
		float Radius;;
		FVector2D Center;
	
		GameState->TimeStormComponent->GetCurrStormZone_UV(Center, Radius);
		StormZoneMID->SetScalarParameterValue(FName{"Radius"}, Radius);
		StormZoneMID->SetVectorParameterValue(FName{"CenterPosition"}, FVector(Center.X, Center.Y, 0));
	
		GameState->TimeStormComponent->GetDestStormZone_UV(Center, Radius);
		NextStormZoneMID->SetScalarParameterValue(FName{"Radius"}, Radius);
		NextStormZoneMID->SetVectorParameterValue(FName{"CenterPosition"}, FVector(Center.X, Center.Y, 0));
	}
}
