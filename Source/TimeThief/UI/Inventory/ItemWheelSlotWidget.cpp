#include "ItemWheelSlotWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"

void UItemWheelSlotWidget::SetData(const FString& Name, UTexture2D* Icon)
{
	if (ItemImage && Icon)
	{
		ItemImage->SetBrushFromTexture(Icon);
	}

	if (ItemText)
	{
		ItemText->SetText(FText::FromString(Name));
	}
}
