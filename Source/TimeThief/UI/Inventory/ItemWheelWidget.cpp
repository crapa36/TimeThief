#include "ItemWheelWidget.h"

#include "ItemWheelSlotWidget.h"
#include "Character/TimeThiefPlayerCharacter.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/System/InventorySystemComponent.h"
#include "DataAssets/GameItemData.h"
#include "Framework/Application/SlateApplication.h"
#include "Game/ItemSettings.h"

namespace
{
	constexpr float DefaultSlotSize = 112.0f;
	constexpr float SingleItemSlotSize = 156.0f;
	constexpr float TwoItemSlotSize = 136.0f;
	constexpr float IconSizeRatio = 0.78f;

	bool IsKnownThrowableItem(EItemID ItemID)
	{
		return ItemID == EItemID::Grenade || ItemID == EItemID::SmokeGrenade;
	}

	bool IsWheelItem(EItemID ItemID, const FItemData* ItemData)
	{
		return IsKnownThrowableItem(ItemID)
			|| (ItemData && (ItemData->Category == EItemCategory::Consumable || ItemData->Category == EItemCategory::Throwable));
	}

	float GetSlotSizeForItemCount(int32 ItemCount)
	{
		if (ItemCount <= 1)
		{
			return SingleItemSlotSize;
		}

		if (ItemCount == 2)
		{
			return TwoItemSlotSize;
		}

		return DefaultSlotSize;
	}

	struct FWheelLayout
	{
		float FirstSlotAngle = -90.0f;
		float SectorAngle = 0.0f;

		float GetSlotAngle(int32 ItemIndex) const
		{
			return FirstSlotAngle + (ItemIndex * SectorAngle);
		}

		float GetDividerAngleAfter(int32 ItemIndex) const
		{
			return GetSlotAngle(ItemIndex) + (SectorAngle * 0.5f);
		}

		FVector2D GetSlotDirection(int32 ItemIndex) const
		{
			const float RadAngle = FMath::DegreesToRadians(GetSlotAngle(ItemIndex));
			return FVector2D(FMath::Cos(RadAngle), FMath::Sin(RadAngle));
		}
	};

	FWheelLayout MakeWheelLayout(int32 ItemCount)
	{
		FWheelLayout Layout;
		Layout.SectorAngle = ItemCount > 0 ? 360.0f / ItemCount : 0.0f;

		if (ItemCount == 2)
		{
			Layout.FirstSlotAngle = 180.0f;
		}
		else if (ItemCount == 3)
		{
			Layout.FirstSlotAngle = -30.0f;
		}

		return Layout;
	}
}

void UItemWheelWidget::SetVisibility(ESlateVisibility InVisibility)
{
	const bool bWasVisible = IsVisible();
	Super::SetVisibility(InVisibility);

	if (InVisibility != ESlateVisibility::Visible)
	{
		if (bWasVisible)
		{
			ApplySelectedItem();
		}

		if (APlayerController* PC = GetOwningPlayer())
		{
			PC->SetIgnoreLookInput(false);
			PC->SetShowMouseCursor(false);
			
			int32 ViewportSizeX, ViewportSizeY;
			PC->GetViewportSize(ViewportSizeX, ViewportSizeY);
			PC->SetMouseLocation(ViewportSizeX / 2, ViewportSizeY / 2);
		}
	}
	else
	{
		BuildWheel();

		if (APlayerController* PC = GetOwningPlayer())
		{
			PC->SetIgnoreLookInput(true);
			PC->SetShowMouseCursor(true);

			int32 ViewportSizeX, ViewportSizeY;
			PC->GetViewportSize(ViewportSizeX, ViewportSizeY);
			PC->SetMouseLocation(ViewportSizeX / 2, ViewportSizeY / 2);
		}
	}
}

void UItemWheelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (WheelBackGround)
	{
		HighlightMID = WheelBackGround->GetDynamicMaterial();
	}

	BuildWheel();
}

void UItemWheelWidget::BuildWheel()
{
	CurrentSelectedIndex = -1;

	if (!WheelCanvas || !SlotWidgetClass)
	{
		return;
	}

	if (WheelBackGround)
	{
		if (auto BorderSlot = Cast<UCanvasPanelSlot>(WheelBackGround->Slot))
		{
			BorderSlot->SetSize(FVector2D(WheelRadius * 2, WheelRadius * 2));
			BorderSlot->SetPosition(FVector2D(0.0, 0.0));
			BorderSlot->SetAnchors(FAnchors(0.5, 0.5));
			BorderSlot->SetAlignment(FVector2D(0.5, 0.5));
		}
	}

	WheelCanvas->ClearChildren();

	const UGameItemData* LoadedData = GetDefault<UItemSettings>()->GetItemData();
	const UInventorySystemComponent* InventoryComponent = nullptr;
	if (const ATimeThiefPlayerCharacter* Player = Cast<ATimeThiefPlayerCharacter>(GetOwningPlayerPawn()))
	{
		InventoryComponent = Player->GetInventoryComponent();
	}

	RebuildVisibleItemList(InventoryComponent, LoadedData);

	const int32 TotalSlots = VisibleItemList.Num();
	CurrentSelectedIndex = TotalSlots == 1 ? 0 : -1;
	RefreshHighlightMaterial();

	if (WheelBackGround)
	{
		WheelBackGround->SetVisibility(TotalSlots > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}

	if (TotalSlots <= 0 || !LoadedData)
	{
		InvalidateLayoutAndVolatility();
		return;
	}

	const float EffectiveSlotSize = GetSlotSizeForItemCount(TotalSlots);
	const FWheelLayout Layout = MakeWheelLayout(TotalSlots);

	for (int32 i = 0; i < TotalSlots; i++)
	{
		const FItemData* ItemStat = LoadedData->Items.Find(VisibleItemList[i]);
		if (!ItemStat)
		{
			continue;
		}

		if (UItemWheelSlotWidget* NewSlot = CreateWidget<UItemWheelSlotWidget>(this, SlotWidgetClass))
		{
			UCanvasPanelSlot* CanvasSlot = WheelCanvas->AddChildToCanvas(NewSlot);

			FVector2D SlotPosition = FVector2D::ZeroVector;
			if (TotalSlots > 1)
			{
				SlotPosition = Layout.GetSlotDirection(i) * (WheelRadius * 0.5f);
			}

			CanvasSlot->SetPosition(SlotPosition);
			CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			CanvasSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			CanvasSlot->SetSize(FVector2D(EffectiveSlotSize, EffectiveSlotSize));

			NewSlot->SetData(ItemStat->Name, ItemStat->Icon, EffectiveSlotSize * IconSizeRatio);
		}
	}

	InvalidateLayoutAndVolatility();
}

void UItemWheelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateSelection(MyGeometry);
}

int32 UItemWheelWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
                                    const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
                                    int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 MaxLayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId,
	                                      InWidgetStyle, bParentEnabled);

	FVector2D Center = AllottedGeometry.GetLocalSize() * 0.5f;
	int32 TotalSlots = VisibleItemList.Num();

	if (TotalSlots <= 0) return MaxLayerId;

	const FWheelLayout Layout = MakeWheelLayout(TotalSlots);

	if (TotalSlots > 1)
	{
		for (int32 n = 0; n < TotalSlots; n++)
		{
			float DividerAngle = Layout.GetDividerAngleAfter(n);
			float RadAngle = FMath::DegreesToRadians(DividerAngle);
			FVector2D EndPoint = Center + FVector2D(FMath::Cos(RadAngle), FMath::Sin(RadAngle)) * (OuterRingRadius +
				WheelRadius);

			TArray<FVector2D> LinePoints;
			LinePoints.Add(Center);
			LinePoints.Add(EndPoint);

			FSlateDrawElement::MakeLines(
				OutDrawElements, MaxLayerId + 3, AllottedGeometry.ToPaintGeometry(),
				LinePoints, ESlateDrawEffect::None, LineColor, true, LineThickness
			);
		}
	}

	TArray<FVector2D> RingPoints;
	for (int32 o = 0; o <= RingSegments; o++)
	{
		float CurrentAngle = (float)o / (float)RingSegments * 360.0f;
		float RadAngle = FMath::DegreesToRadians(CurrentAngle);
		FVector2D Point = Center + FVector2D(FMath::Cos(RadAngle), FMath::Sin(RadAngle)) * (OuterRingRadius +
			WheelRadius);
		RingPoints.Add(Point);
	}

	FSlateDrawElement::MakeLines(
		OutDrawElements, MaxLayerId + 3, AllottedGeometry.ToPaintGeometry(),
		RingPoints, ESlateDrawEffect::None, LineColor, true, LineThickness
	);

	return MaxLayerId + 3;
}

void UItemWheelWidget::ApplySelectedItem()
{
	if (!VisibleItemList.IsValidIndex(CurrentSelectedIndex))
	{
		return;
	}

	if (ATimeThiefPlayerCharacter* Player = Cast<ATimeThiefPlayerCharacter>(GetOwningPlayerPawn()))
	{
		if (UInventorySystemComponent* InventoryComponent = Player->GetInventoryComponent())
		{
			InventoryComponent->SetEquipment(VisibleItemList[CurrentSelectedIndex]);
		}
	}
}

void UItemWheelWidget::RebuildVisibleItemList(const UInventorySystemComponent* InventoryComponent, const UGameItemData* ItemData)
{
	VisibleItemList.Reset();
	if (!InventoryComponent || !ItemData)
	{
		return;
	}

	auto TryAddItem = [this, InventoryComponent, ItemData](EItemID ItemID)
	{
		if (ItemID == EItemID::SIZE || VisibleItemList.Contains(ItemID) || InventoryComponent->GetItemQuantity(ItemID) <= 0)
		{
			return;
		}

		const FItemData* ItemStat = ItemData->Items.Find(ItemID);
		if (IsWheelItem(ItemID, ItemStat))
		{
			VisibleItemList.Add(ItemID);
		}
	};

	if (ItemList.Num() > 0)
	{
		for (EItemID ItemID : ItemList)
		{
			TryAddItem(ItemID);
		}
	}

	for (EItemID ItemID : TEnumRange<EItemID>())
	{
		if (ItemList.Contains(ItemID))
		{
			continue;
		}

		TryAddItem(ItemID);
	}
}

void UItemWheelWidget::SetCurrentSelectedIndex(int32 NewSelectedIndex)
{
	if (CurrentSelectedIndex == NewSelectedIndex)
	{
		return;
	}

	CurrentSelectedIndex = NewSelectedIndex;
	RefreshHighlightMaterial();
}

void UItemWheelWidget::RefreshHighlightMaterial()
{
	if (!HighlightMID)
	{
		return;
	}

	const int32 TotalSlots = VisibleItemList.Num();
	const FWheelLayout Layout = MakeWheelLayout(TotalSlots);
	if (TotalSlots <= 0 || !VisibleItemList.IsValidIndex(CurrentSelectedIndex))
	{
		HighlightMID->SetScalarParameterValue(FName("HalfAngleRad"), 0.0f);
		HighlightMID->SetVectorParameterValue(FName("HighlightDir"), FLinearColor(0.0f, -1.0f, 0.0f, 0.0f));
		return;
	}

	const float HalfAngleRad = FMath::DegreesToRadians(Layout.SectorAngle * 0.5f);
	HighlightMID->SetScalarParameterValue(FName("HalfAngleRad"), HalfAngleRad);

	const FVector2D Direction = Layout.GetSlotDirection(CurrentSelectedIndex);
	HighlightMID->SetVectorParameterValue(FName("HighlightDir"), FLinearColor(Direction.X, Direction.Y, 0.0f, 0.0f));
}

void UItemWheelWidget::UpdateSelection(const FGeometry& WheelGeometry)
{
	if (!IsVisible())
	{
		return;
	}

	const int32 TotalSlots = VisibleItemList.Num();
	if (TotalSlots <= 0)
	{
		SetCurrentSelectedIndex(-1);
		return;
	}

	if (TotalSlots == 1)
	{
		SetCurrentSelectedIndex(0);
		return;
	}

	FVector2D AbsoluteMousePos = FSlateApplication::Get().GetCursorPos();
	
	FVector2D LocalMousePos = WheelGeometry.AbsoluteToLocal(AbsoluteMousePos);
	FVector2D Center = WheelGeometry.GetLocalSize() * 0.5f;
	
	float Distance = FVector2D::Distance(LocalMousePos, Center);
	if (Distance < 10.0f)
	{
		SetCurrentSelectedIndex(-1);
		return;
	}
	
	FVector2D Dir = LocalMousePos - Center;
	Dir.Normalize();

	float BestDot = -1.0f;
	int32 BestIndex = -1;
	const FWheelLayout Layout = MakeWheelLayout(TotalSlots);
	for (int32 i = 0; i < TotalSlots; ++i)
	{
		const FVector2D SlotDir = Layout.GetSlotDirection(i);
		const float Dot = FVector2D::DotProduct(Dir, SlotDir);
		if (Dot > BestDot)
		{
			BestDot = Dot;
			BestIndex = i;
		}
	}
	SetCurrentSelectedIndex(BestIndex);
}
