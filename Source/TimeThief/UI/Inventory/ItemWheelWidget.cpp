#include "ItemWheelWidget.h"

#include "ItemWheelSlotWidget.h"
#include "Character/TimeThiefPlayerCharacter.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/System/InventorySystemComponent.h"
#include "Game/ItemSettings.h"
#include "Kismet/KismetSystemLibrary.h"

void UItemWheelWidget::SetVisibility(ESlateVisibility InVisibility)
{
	Super::SetVisibility(InVisibility);

	if (InVisibility == ESlateVisibility::Hidden)
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			PC->SetIgnoreLookInput(false);
			PC->SetShowMouseCursor(false);
			
			int32 ViewportSizeX, ViewportSizeY;
			PC->GetViewportSize(ViewportSizeX, ViewportSizeY);
			PC->SetMouseLocation(ViewportSizeX / 2, ViewportSizeY / 2);
			
			if (CurrentSelectedIndex != -1)
			{
				if (auto* Player = Cast<ATimeThiefPlayerCharacter>(GetOwningPlayerPawn()))
				{
					if (auto Inven = Player->GetInventoryComponent())
					{
						Inven->SetEquipment(ItemList[CurrentSelectedIndex]);
					}
				}
			}
		}
	}
	else if (InVisibility == ESlateVisibility::Visible)
	{
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
	BuildWheel();

	HighlightMID = WheelBackGround->GetDynamicMaterial();
	
	float SectorSize = 360.0f / ItemList.Num();
	float HalfAngleRad = FMath::DegreesToRadians(SectorSize * 0.5f);
	HighlightMID->SetScalarParameterValue(FName("HalfAngleRad"), HalfAngleRad);
}

void UItemWheelWidget::BuildWheel()
{
	if (!WheelCanvas || !SlotWidgetClass) return;

	if (auto BorderSlot = Cast<UCanvasPanelSlot>(WheelBackGround->Slot))
	{
		BorderSlot->SetSize(FVector2D(WheelRadius * 2, WheelRadius * 2));
		BorderSlot->SetPosition(FVector2D(0.0, 0.0));
		BorderSlot->SetAnchors(FAnchors(0.5, 0.5));
		BorderSlot->SetAlignment(FVector2D(0.5, 0.5));
	}

	WheelCanvas->ClearChildren();

	int32 TotalSlots = ItemList.Num();
	if (TotalSlots <= 0) return;

	float AngleStep = 360.0f / TotalSlots;

	for (int32 i = 0; i < TotalSlots; i++)
	{
		if (UItemWheelSlotWidget* NewSlot = CreateWidget<UItemWheelSlotWidget>(this, SlotWidgetClass))
		{
			UCanvasPanelSlot* CanvasSlot = WheelCanvas->AddChildToCanvas(NewSlot);

			// 12시 방향부터 시작하도록 -90도 보정
			float CurrentAngle = (i * AngleStep) - 90.0f;
			float RadAngle = FMath::DegreesToRadians(CurrentAngle);

			// 삼각함수로 좌표 계산 (X = cos, Y = sin)
			float PosX = FMath::Cos(RadAngle) * WheelRadius / 2;
			float PosY = FMath::Sin(RadAngle) * WheelRadius / 2;

			// 중앙 정렬 배치
			CanvasSlot->SetPosition(FVector2D(PosX, PosY));
			CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			CanvasSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			
			double Section = WheelRadius / TotalSlots;
			CanvasSlot->SetSize(FVector2D(Section * 2, Section * 2)); // 아이콘 크기
			
			const UItemSettings* StoreSettings = GetDefault<UItemSettings>();
			if (UGameItemData* LoadedData = StoreSettings->ItemData.LoadSynchronous())
			{
				const FItemData& ItemStat = LoadedData->Items[ItemList[i]];
				NewSlot->SetData(ItemStat.Name, ItemStat.Icon);
			}
		}
	}
}

void UItemWheelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateSelection();
}

int32 UItemWheelWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
                                    const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
                                    int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 MaxLayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId,
	                                      InWidgetStyle, bParentEnabled);

	FVector2D Center = AllottedGeometry.GetLocalSize() * 0.5f;
	int32 TotalSlots = ItemList.Num();

	if (TotalSlots <= 0) return MaxLayerId;

	float AngleStep = 360.0f / TotalSlots;

	// --- 1. 슬롯 사이 구분선 그리기 ---
	for (int32 n = 0; n < TotalSlots; n++)
	{
		float DividerAngle = (n * AngleStep) - 90.0f + (AngleStep * 0.5f);
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

	// --- 2. 외곽 휠 테두리 원형 그리기 ---
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

void UItemWheelWidget::UpdateSelection()
{
	FVector2D AbsoluteMousePos = FSlateApplication::Get().GetCursorPos();
	
	FVector2D LocalMousePos = GetCachedGeometry().AbsoluteToLocal(AbsoluteMousePos);
	FVector2D Center = GetCachedGeometry().GetLocalSize() * 0.5f;
	
	float Distance = FVector2D::Distance(LocalMousePos, Center);
	if (Distance < 10.0f)
	{
		CurrentSelectedIndex = -1;
		return;
	}
	
	FVector2D Dir = LocalMousePos - Center;
	float AtanDegree = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
	
	float FinalAngle = FMath::Fmod(AtanDegree + 90.0f + 360.0f, 360.0f);
	
	int32 TotalSlots = ItemList.Num();
	if (TotalSlots > 0)
	{
		float SectorSize = 360.0f / TotalSlots;
		CurrentSelectedIndex = FMath::FloorToInt((FinalAngle + (SectorSize * 0.5f)) / SectorSize) % TotalSlots;
	}

	if (HighlightMID && CurrentSelectedIndex != -1 && TotalSlots > 0)
	{
		float SectorSize = 360.0f / TotalSlots;
		
		float TargetAngle = (CurrentSelectedIndex * SectorSize) - 90.0f;
		float RadAngle = FMath::DegreesToRadians(TargetAngle);
		
		FLinearColor DirColor(FMath::Cos(RadAngle), FMath::Sin(RadAngle), 0.0f, 0.0f);
		HighlightMID->SetVectorParameterValue(FName("HighlightDir"), DirColor);
	}
}
