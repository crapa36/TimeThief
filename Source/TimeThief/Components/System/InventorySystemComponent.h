// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ItemCommons.h"
#include "Components/ActorComponent.h"
#include "InventorySystemComponent.generated.h"

DECLARE_MULTICAST_DELEGATE(FOnInventoryObjectUpdatedEvent);

UCLASS(Blueprintable)
class UInventoryObject : public UObject
{
	GENERATED_BODY()

public:
	EItemID ItemID;

	int Quantity = 0;

	FOnInventoryObjectUpdatedEvent OnInventoryObjectUpdatedEvent;
};

DECLARE_MULTICAST_DELEGATE(FOnInventoryUpdatedEvent);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class TIMETHIEF_API UInventorySystemComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UInventorySystemComponent();

	FOnInventoryUpdatedEvent OnInventoryUpdatedEvent;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	virtual void OnRegister() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	void AddItem(EItemID Item, int Amount = 1);
	bool RemoveItem(EItemID Item, int Amount = 1);

	const TArray<TObjectPtr<UInventoryObject>>& GetInventory() const { return ItemQuantities; }

	void SetEquipment(EItemID Item) { Equipment = Item; }
	EItemID GetEquipment() const { return Equipment; }

private:
	UPROPERTY()
	TArray<TObjectPtr<UInventoryObject>> ItemQuantities;

	UPROPERTY()
	EItemID Equipment;
};
