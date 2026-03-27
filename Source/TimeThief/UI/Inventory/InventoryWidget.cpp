// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryWidget.h"

#include "Character/TimeThiefPlayerCharacter.h"
#include "Components/ListView.h"
#include "Actors/Item/ItemBase.h"
#include "Kismet/KismetSystemLibrary.h"

void UInventoryWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	
	if (IsVisible() && Player.IsValid())
	{
		const auto& VicinityItem = Player->GetVicinityItems();
		VicinityItem_ListView->SetListItems(VicinityItem);
	}
}

void UInventoryWidget::Init(ATimeThiefPlayerCharacter* InPlayer)
{
	Player = InPlayer;
}
