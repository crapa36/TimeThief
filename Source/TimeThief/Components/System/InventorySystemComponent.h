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
DECLARE_MULTICAST_DELEGATE_OneParam(FOnEquipmentUpadatedEvent, EItemID);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class TIMETHIEF_API UInventorySystemComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UInventorySystemComponent();

	FOnInventoryUpdatedEvent OnInventoryUpdatedEvent;
	FOnEquipmentUpadatedEvent OnConsumableEquipmentUpdatedEvent;
	FOnEquipmentUpadatedEvent OnThrowableEquipmentUpdatedEvent;
	
protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	virtual void OnRegister() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	void AddItem(EItemID ItemID, int Amount = 1);
	bool RemoveItem(EItemID ItemID, int Amount = 1);
	
	void SetInventory(const TArray<TPair<EItemID,int>>& NewInventory);
	const TArray<TObjectPtr<UInventoryObject>>& GetInventory() const { return ItemQuantities; }

	void SetEquipment(EItemID ItemID);

	UFUNCTION(BlueprintPure, Category = "TimeThief|Inventory")
	UInventoryObject* FindInventoryObject(EItemID ItemID) const;

	UFUNCTION(BlueprintPure, Category = "TimeThief|Inventory")
	int GetItemQuantity(EItemID ItemID) const;

	UFUNCTION(BlueprintPure, Category = "TimeThief|Inventory")
	EItemID GetThrowableEquipment() const { return ThrowableEquipment; }

	UFUNCTION(BlueprintPure, Category = "TimeThief|Inventory")
	EItemID GetConsumableEquipment() const { return ConsumableEquipment; }
	
private:
	void SetConsumableEquipment(EItemID ItemID);
	void SetThrowableEquipment(EItemID ItemID);
	
	UPROPERTY()
	TArray<TObjectPtr<UInventoryObject>> ItemQuantities;
	
	EItemID ConsumableEquipment = EItemID::SIZE;
	EItemID ThrowableEquipment = EItemID::SIZE;
};
