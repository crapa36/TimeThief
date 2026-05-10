// Fill out your copyright notice in the Description page of Project Settings.


#include "TimePointItemActor.h"

#include "Character/TimeThiefPlayerCharacter.h"
#include "Components/System/TimePointSystemComponent.h"


// Sets default values
ATimePointItemActor::ATimePointItemActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
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
	
	MeshComponent->AddLocalRotation(FRotator{0, DeltaTime * FMath::DegreesToRadians(3600), 0});
}

void ATimePointItemActor::Interact(const ATimeThiefPlayerCharacter* Player)
{
	if (UTimePointSystemComponent* TP = Player->GetComponentByClass<UTimePointSystemComponent>())
	{
		TP->ModifyTimePoints(Quantity);
		Disable();
	}
}

