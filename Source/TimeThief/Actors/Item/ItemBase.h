// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "../InteractionActorBase.h"
#include "ItemCommons.h"
#include "ItemBase.generated.h"

UCLASS(Blueprintable, BlueprintType)
class TIMETHIEF_API AItemBase : public AInteractionActorBase
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<USphereComponent> LookingSphere;

public:
	// Sets default values for this actor's properties
	AItemBase();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	virtual void Interact(const ATimeThiefPlayerCharacter* Player) override;

	virtual void OnBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor, 
		UPrimitiveComponent* OtherComponent, 
		int32 OtherBodyIndex, 
		bool bFromSweep,
		const FHitResult& SweepResult) override;
	
	virtual void OnEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex
	) override;


	EItemID GetItemID() const { return ItemID; }
	int GetQuantity() const { return Quantity; }
	
private:
	void TryRequestServer();
	
public:
	void SetItemStack(EItemID NewItemID, int NewQuantity);

protected:
	UPROPERTY(EditAnywhere, meta=(AllowPrivateAccess=true), Category="Item")
	EItemID ItemID;

	UPROPERTY(EditAnywhere, meta=(AllowPrivateAccess=true), Category="Item")
	int Quantity = 1;
};
