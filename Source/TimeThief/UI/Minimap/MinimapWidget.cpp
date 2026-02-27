// Fill out your copyright notice in the Description page of Project Settings.


#include "MinimapWidget.h"

#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/System/TimeStormComponent.h"
#include "Game/TimeThiefGameState.h"

void UMinimapWidget::NativeConstruct()
{
	Super::NativeConstruct();

	Player = GetOwningPlayerPawn();
	MinimapSize = Cast<UCanvasPanelSlot>(Minimap_Image->Slot)->GetSize();

	StormZoneDMI = StormZone_Image->GetDynamicMaterial();
	NextStormZoneDMI = NextStormZone_Image->GetDynamicMaterial();
	
	GameState = GetWorld()->GetGameState<ATimeThiefGameState>();
}

void UMinimapWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const FVector2f& MapSize = GameState->TimeStormComponent->MapSize;
	
	if (IsVisible() && IsValid(Player))
	{
		const FVector PlayerLocation = Player->GetActorLocation();
		FVector2D MinimapPosition;
		MinimapPosition.X = PlayerLocation.Y / MapSize.Y * MinimapSize.X;
		MinimapPosition.Y = PlayerLocation.X / MapSize.X * -MinimapSize.Y;

		Player_Icon->SetRenderTranslation(MinimapPosition);
	}
	float Radius;;
	FVector2D Center;
	
	GameState->TimeStormComponent->GetCurrStormZone_UV(Center, Radius);
	StormZoneDMI->SetScalarParameterValue(FName{"Radius"}, Radius);
	StormZoneDMI->SetVectorParameterValue(FName{"CenterPosition"}, FVector(Center.X, Center.Y, 0));
	
	GameState->TimeStormComponent->GetDestStormZone_UV(Center, Radius);
	NextStormZoneDMI->SetScalarParameterValue(FName{"Radius"}, Radius);
	NextStormZoneDMI->SetVectorParameterValue(FName{"CenterPosition"}, FVector(Center.X, Center.Y, 0));
}
