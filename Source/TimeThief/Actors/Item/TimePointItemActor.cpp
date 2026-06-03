// Fill out your copyright notice in the Description page of Project Settings.


#include "TimePointItemActor.h"

#include "Character/TimeThiefPlayerCharacter.h"
#include "Components/ItemMovementComponent.h"
#include "Components/System/TimePointSystemComponent.h"


// Sets default values
ATimePointItemActor::ATimePointItemActor()
{
	PrimaryActorTick.bCanEverTick = false;

	ItemMovementComponent = CreateDefaultSubobject<UItemMovementComponent>("ItemMovementComponent");
}

// Called when the game starts or when spawned
void ATimePointItemActor::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void ATimePointItemActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ATimePointItemActor::Interact(const ATimeThiefPlayerCharacter* Player)
{
	if (UTimePointSystemComponent* TP = Player->GetComponentByClass<UTimePointSystemComponent>())
	{
		TP->ModifyTimePoints(Quantity);
		Disable();
	}
}

