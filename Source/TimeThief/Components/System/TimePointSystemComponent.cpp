// Fill out your copyright notice in the Description page of Project Settings.


#include "TimePointSystemComponent.h"

#include "TimeStormComponent.h"
#include "Character/TimeThiefCharacterBase.h"
#include "Game/TimeThiefGameState.h"


// Sets default values for this component's properties
UTimePointSystemComponent::UTimePointSystemComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	
	OnTimePointsChanged_Delegate.AddUObject(this, &UTimePointSystemComponent::HandleTimePointsChanged);
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
				TimePoints -= DeltaTime * TimePointsGainPerSecond * 2 * (static_cast<int>(DamagedElapsedTime) / 10 + 1);
				DamagedElapsedTime += DeltaTime;
				RecoveredElapsedTime = 0;
				if (auto Character = Cast<ATimeThiefCharacterBase>(GetOwner()))
				{
					float Mask = 1 - DamagedElapsedTime / 50.0f * std::clamp(TimePoints, 0.f, DangerThreshold) / DangerThreshold;
					Character->SetMask(std::clamp(Mask, 0.2f, 1.f));
				}
			}
			else
			{
				DamagedElapsedTime = 0;
				if (auto Character = Cast<ATimeThiefCharacterBase>(GetOwner()))
				{
					Character->AddMask(DeltaTime * std::clamp(TimePoints, 0.f, DangerThreshold) / DangerThreshold / 5);
				}
			}
		}
	}
	
	TimePoints += DeltaTime * TimePointsGainPerSecond;
	
	if (LastDisplayTimePoints != GetTimePoints())
	{
		OnTimePointsChanged_Delegate.Broadcast(GetTimePoints());
	}
}

bool UTimePointSystemComponent::ModifyTimePoints(int Value)
{
	if (TimePoints + Value >= 0)
	{
		TimePoints += Value;
		OnTimePointsChanged_Delegate.Broadcast(GetTimePoints());
		return true;
	}
	return false;
}

void UTimePointSystemComponent::HandleTimePointsChanged(int InTimePoints)
{
	LastDisplayTimePoints = InTimePoints;
}

