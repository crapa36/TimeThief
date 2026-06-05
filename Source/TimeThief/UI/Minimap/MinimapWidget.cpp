// Fill out your copyright notice in the Description page of Project Settings.


#include "MinimapWidget.h"

#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/System/TimeStormComponent.h"
#include "Game/TimeThiefGameState.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

void UMinimapWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	if (Minimap_Image)
	{
		if (const UCanvasPanelSlot* MinimapSlot = Cast<UCanvasPanelSlot>(Minimap_Image->Slot))
		{
			MinimapSize = MinimapSlot->GetSize();
		}
	}

	StormZoneMID = StormZone_Image ? StormZone_Image->GetDynamicMaterial() : nullptr;
	NextStormZoneMID = NextStormZone_Image ? NextStormZone_Image->GetDynamicMaterial() : nullptr;

	if (Player_Icon)
	{
		Player_Icon->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	}
	
	if (UWorld* World = GetWorld())
	{
		GameState = World->GetGameState<ATimeThiefGameState>();
	}
}

void UMinimapWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!IsVisible())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	GameState = World->GetGameState<ATimeThiefGameState>();
	if (!IsValid(GameState) || !IsValid(GameState->TimeStormComponent))
	{
		return;
	}

	const FVector2f& MapSize = GameState->TimeStormComponent->MapSize;
	if (FMath::IsNearlyZero(MapSize.X) || FMath::IsNearlyZero(MapSize.Y))
	{
		return;
	}

	const APlayerController* PlayerController = World->GetFirstPlayerController();
	if (!IsValid(PlayerController))
	{
		return;
	}

	const APawn* Pawn = PlayerController->GetPawn();
	if (!IsValid(Pawn))
	{
		return;
	}

	const FVector PlayerLocation = Pawn->GetActorLocation();
	FVector2D MinimapPosition;
	MinimapPosition.X = PlayerLocation.Y / MapSize.Y * MinimapSize.X;
	MinimapPosition.Y = PlayerLocation.X / MapSize.X * -MinimapSize.Y;

	if (Player_Icon)
	{
		Player_Icon->SetRenderTranslation(MinimapPosition);
		Player_Icon->SetRenderTransformAngle(PlayerController->GetControlRotation().Yaw);
	}
		
	float Radius;
	FVector2D Center;
	
	if (StormZoneMID)
	{
		GameState->TimeStormComponent->GetCurrStormZone_UV(Center, Radius);
		StormZoneMID->SetScalarParameterValue(FName{"Radius"}, Radius);
		StormZoneMID->SetVectorParameterValue(FName{"CenterPosition"}, FVector(Center.X, Center.Y, 0));
	}

	if (NextStormZoneMID)
	{
		GameState->TimeStormComponent->GetDestStormZone_UV(Center, Radius);
		NextStormZoneMID->SetScalarParameterValue(FName{"Radius"}, Radius);
		NextStormZoneMID->SetVectorParameterValue(FName{"CenterPosition"}, FVector(Center.X, Center.Y, 0));
	}
}
