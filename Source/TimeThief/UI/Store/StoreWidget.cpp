// Fill out your copyright notice in the Description page of Project Settings.


#include "StoreWidget.h"

#include "Kismet/KismetSystemLibrary.h"

void UStoreWidget::SetVisibility(ESlateVisibility InVisibility)
{
	Super::SetVisibility(InVisibility);

	if (InVisibility == ESlateVisibility::Hidden)
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			PC->SetIgnoreLookInput(false);
			PC->SetInputMode(FInputModeGameOnly{});
			PC->bShowMouseCursor = false;
		}
	}
	else if (InVisibility == ESlateVisibility::Visible)
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			// UKismetSystemLibrary::PrintString(this, TEXT("Store Opened"));
			
			PC->SetIgnoreLookInput(true);

			PC->SetInputMode(FInputModeGameAndUI{}
				.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock)
				);
			PC->bShowMouseCursor = true;

			int32 ViewportSizeX, ViewportSizeY;
			PC->GetViewportSize(ViewportSizeX, ViewportSizeY);
			PC->SetMouseLocation(ViewportSizeX / 2, ViewportSizeY / 2);
		}
	}
}
