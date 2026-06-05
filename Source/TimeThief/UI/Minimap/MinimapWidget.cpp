// Fill out your copyright notice in the Description page of Project Settings.


#include "MinimapWidget.h"

#include "Actors/StoreActor.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/System/TimeStormComponent.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "Game/TimeThiefGameState.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Network/NetworkGameInstanceSubsystem.h"

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
	MinimapCanvas = Minimap_Image ? Cast<UCanvasPanel>(Minimap_Image->GetParent()) : nullptr;

	if (Player_Icon)
	{
		Player_Icon->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));

		if (UCanvasPanelSlot* PlayerSlot = Cast<UCanvasPanelSlot>(Player_Icon->Slot))
		{
			PlayerSlot->SetZOrder(20);
		}
	}
	
	if (UWorld* World = GetWorld())
	{
		GameState = World->GetGameState<ATimeThiefGameState>();
	}
}

void UMinimapWidget::NativeDestruct()
{
	ClearStoreIcons();
	MinimapCanvas = nullptr;

	Super::NativeDestruct();
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
	const FVector2D MinimapPosition = WorldToMinimapPosition(PlayerLocation, MapSize);

	if (Player_Icon)
	{
		Player_Icon->SetRenderTranslation(MinimapPosition);
		Player_Icon->SetRenderTransformAngle(PlayerController->GetControlRotation().Yaw);
	}

	UpdateStoreIcons(World, MapSize);
		
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

FVector2D UMinimapWidget::WorldToMinimapPosition(const FVector& WorldLocation, const FVector2f& MapSize) const
{
	FVector2D MinimapPosition;
	MinimapPosition.X = WorldLocation.Y / MapSize.Y * MinimapSize.X;
	MinimapPosition.Y = WorldLocation.X / MapSize.X * -MinimapSize.Y;
	return MinimapPosition;
}

void UMinimapWidget::GatherStoreActors(UWorld* World, TArray<AStoreActor*>& OutStoreActors) const
{
	OutStoreActors.Reset();
	if (!World)
	{
		return;
	}

	UNetworkGameInstanceSubsystem* NetworkSubsystem = UNetworkGameInstanceSubsystem::Get(World);
	if (NetworkSubsystem && NetworkSubsystem->IsConnected())
	{
		NetworkSubsystem->GetStoreActors(OutStoreActors);
		return;
	}

	for (TActorIterator<AStoreActor> StoreIt(World); StoreIt; ++StoreIt)
	{
		if (AStoreActor* StoreActor = *StoreIt)
		{
			if (IsValid(StoreActor))
			{
				OutStoreActors.Add(StoreActor);
			}
		}
	}
}

void UMinimapWidget::UpdateStoreIcons(UWorld* World, const FVector2f& MapSize)
{
	TArray<AStoreActor*> StoreActors;
	GatherStoreActors(World, StoreActors);

	TSet<TWeakObjectPtr<AStoreActor>> ActiveStores;
	for (AStoreActor* StoreActor : StoreActors)
	{
		if (!IsValid(StoreActor))
		{
			continue;
		}

		ActiveStores.Add(StoreActor);

		if (UImage* StoreIcon = GetOrCreateStoreIcon(StoreActor))
		{
			StoreIcon->SetRenderTranslation(WorldToMinimapPosition(StoreActor->GetActorLocation(), MapSize));
		}
	}

	RemoveStaleStoreIcons(ActiveStores);
}

UImage* UMinimapWidget::GetOrCreateStoreIcon(AStoreActor* StoreActor)
{
	if (!IsValid(StoreActor))
	{
		return nullptr;
	}

	const TWeakObjectPtr<AStoreActor> StoreKey(StoreActor);
	if (TObjectPtr<UImage>* FoundIcon = StoreIcons.Find(StoreKey))
	{
		return FoundIcon->Get();
	}

	if (!StoreIconTexture || !MinimapCanvas)
	{
		return nullptr;
	}

	UImage* StoreIcon = NewObject<UImage>(this);
	if (!StoreIcon)
	{
		return nullptr;
	}

	StoreIcon->SetBrushFromTexture(StoreIconTexture, false);
	StoreIcon->SetColorAndOpacity(FLinearColor::White);
	StoreIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
	StoreIcon->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));

	UCanvasPanelSlot* StoreSlot = MinimapCanvas->AddChildToCanvas(StoreIcon);
	if (StoreSlot)
	{
		if (const UCanvasPanelSlot* PlayerSlot = Player_Icon ? Cast<UCanvasPanelSlot>(Player_Icon->Slot) : nullptr)
		{
			StoreSlot->SetAnchors(PlayerSlot->GetAnchors());
			StoreSlot->SetAlignment(PlayerSlot->GetAlignment());
			StoreSlot->SetPosition(PlayerSlot->GetPosition());
		}
		else
		{
			StoreSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		}

		StoreSlot->SetAutoSize(false);
		StoreSlot->SetSize(StoreIconSize);
		StoreSlot->SetZOrder(15);
	}

	StoreIcons.Add(StoreKey, StoreIcon);
	return StoreIcon;
}

void UMinimapWidget::RemoveStaleStoreIcons(const TSet<TWeakObjectPtr<AStoreActor>>& ActiveStores)
{
	for (auto It = StoreIcons.CreateIterator(); It; ++It)
	{
		if (It.Key().IsValid() && ActiveStores.Contains(It.Key()))
		{
			continue;
		}

		if (UImage* StoreIcon = It.Value().Get())
		{
			StoreIcon->RemoveFromParent();
		}

		It.RemoveCurrent();
	}
}

void UMinimapWidget::ClearStoreIcons()
{
	for (const TPair<TWeakObjectPtr<AStoreActor>, TObjectPtr<UImage>>& Pair : StoreIcons)
	{
		if (UImage* StoreIcon = Pair.Value.Get())
		{
			StoreIcon->RemoveFromParent();
		}
	}

	StoreIcons.Empty();
}
