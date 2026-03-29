// Fill out your copyright notice in the Description page of Project Settings.


#include "StoreActor.h"

#include "Character/TimeThiefPlayerCharacter.h"
#include "Character/TimeThiefPlayerController.h"
#include "Components/SphereComponent.h"


// Sets default values
AStoreActor::AStoreActor()
{
	Priority = 0;
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AStoreActor::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void AStoreActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AStoreActor::OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                               UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex)
{
	if (ATimeThiefPlayerCharacter* Player = Cast<ATimeThiefPlayerCharacter>(OtherActor))
	{
		if (ATimeThiefPlayerController* PC = Cast<ATimeThiefPlayerController>(Player->GetController()))
		{
			PC->SetStoreVisibility(false);
		}
	}
}

void AStoreActor::Interact(const ATimeThiefPlayerCharacter* Player)
{
	if (ATimeThiefPlayerController* PC = Cast<ATimeThiefPlayerController>(Player->GetController()))
	{
		PC->SetStoreVisibility(true);
	}
}
