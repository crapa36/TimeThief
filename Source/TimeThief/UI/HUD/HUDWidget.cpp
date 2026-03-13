// Fill out your copyright notice in the Description page of Project Settings.


#include "HUDWidget.h"

#include "Character/TimeThiefCharacterBase.h"
#include "Components/TextBlock.h"
#include "Components/System/TimePointSystemComponent.h"

void UHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	
	if (const ATimeThiefCharacterBase* Player = Cast<ATimeThiefCharacterBase>(GetOwningPlayerPawn()))
	{
		if (const UTimePointSystemComponent* TPSComp = Player->GetComponentByClass<UTimePointSystemComponent>())
		{
			TimePoint_Text->SetText(FText::AsNumber(TPSComp->GetTimePoints()));
		}
	}
}
