// Fill out your copyright notice in the Description page of Project Settings.


#include "InventorySystemComponent.h"

#include "Kismet/KismetSystemLibrary.h"


// Sets default values for this component's properties
UInventorySystemComponent::UInventorySystemComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	for (EItemID Item : TEnumRange<EItemID>())
	{
		ItemQuantities.Add(Item, 0);
	}
}


// Called when the game starts
void UInventorySystemComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UInventorySystemComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                              FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UInventorySystemComponent::AddItem(EItemID Item, int Amount)
{
	ItemQuantities[Item] += Amount;
}

bool UInventorySystemComponent::RemoveItem(EItemID Item, int Amount)
{
	if (ItemQuantities[Item] >= Amount)
	{
		ItemQuantities[Item] -= Amount;
		return true;
	}
	
	return false;
}

