#include "ItemWheelSlotWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"

void UItemWheelSlotWidget::SetData(const FString& Name, UTexture2D* Icon, float IconSize)
{
	if (ItemImage && Icon)
	{
		ItemImage->SetBrushFromTexture(Icon);
		if (IconSize > 0.0f)
		{
			ItemImage->SetDesiredSizeOverride(FVector2D(IconSize, IconSize));
		}
	}

	if (ItemText)
	{
		ItemText->SetText(FText::FromString(Name));
	}
}
