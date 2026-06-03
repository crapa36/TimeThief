// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ItemBase.h"
#include "TimePointItemActor.generated.h"

class UItemMovementComponent;

UCLASS()
class TIMETHIEF_API ATimePointItemActor : public AItemBase
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item")
	TObjectPtr<UItemMovementComponent> ItemMovementComponent;

public:
	// Sets default values for this actor's properties
	ATimePointItemActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	virtual void Interact(const ATimeThiefPlayerCharacter* Player) override;
};
