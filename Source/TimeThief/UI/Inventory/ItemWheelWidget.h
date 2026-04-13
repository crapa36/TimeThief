#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/CanvasPanel.h"
#include "ItemCommons.h"
#include "ItemWheelWidget.generated.h"

class UBorder;
class UItemWheelSlotWidget;

UCLASS()
class TIMETHIEF_API UItemWheelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	TArray<EItemID> ItemList;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	TSubclassOf<UItemWheelSlotWidget> SlotWidgetClass;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	float WheelRadius = 250.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Line")
	float OuterRingRadius = 100.0f;          // 외곽 원의 반지름 (EndDistance보다 조금 더 크게 설정)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Line")
	int32 RingSegments = 64;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Line")
	float LineThickness = 2.0f; // 선의 두께

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Line")
	FLinearColor LineColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.3f);
	
	virtual void SetVisibility(ESlateVisibility InVisibility) override;

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> WheelCanvas;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> WheelBackGround;
	
	// 런타임에 수치를 조절할 다이내믹 머티리얼
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HighlightMID;
	
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	int32 CurrentSelectedIndex = -1;

	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	
	UFUNCTION(BlueprintCallable, Category = "Emote")
	void BuildWheel();
	
	void UpdateSelection();
};