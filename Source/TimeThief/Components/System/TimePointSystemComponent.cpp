// Fill out your copyright notice in the Description page of Project Settings.


#include "TimePointSystemComponent.h"

#include "TimeStormComponent.h"
#include "Game/TimeThiefGameState.h"


// Sets default values for this component's properties
UTimePointSystemComponent::UTimePointSystemComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UTimePointSystemComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UTimePointSystemComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                              FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (const ATimeThiefGameState* GameState = GetWorld()->GetGameState<ATimeThiefGameState>())
	{
		if (const UTimeStormComponent* TimeStormComponent= GameState->TimeStormComponent)
		{
			FVector2D Center;
			float Radius;
		
			TimeStormComponent->GetCurrStormZone(Center, Radius);
		
			if (FVector::DistSquaredXY(FVector{Center,0}, GetOwner()->GetActorLocation()) >= Radius * Radius)
			{
				TimePoints -= DeltaTime * TimePointsGainPerSecond * 2;
			}
		}
	}
	
	TimePoints += DeltaTime * TimePointsGainPerSecond;
}

