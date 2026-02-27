// Fill out your copyright notice in the Description page of Project Settings.


#include "TimeStormComponent.h"


// Sets default values for this component's properties
UTimeStormComponent::UTimeStormComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	
}


// Called when the game starts
void UTimeStormComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}

void UTimeStormComponent::OnRegister()
{
	Super::OnRegister();
	
	
	CurrCenter = FVector2D{0.0f};
	PrevCenter = CurrCenter;
	CurrRadius = MapSize.X * 1.5f;
	PrevRadius = CurrRadius;
}


// Called every frame
void UTimeStormComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                        FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bIsShrinking)
	{
		ElapsedTime += DeltaTime;
		float Alpha = FMath::Min(ElapsedTime / ShrinkDuration, 1.0f);
		
		CurrRadius = FMath::Lerp(PrevRadius, DestRadius, Alpha);
		CurrCenter = FMath::Lerp(PrevCenter, DestCenter, Alpha);
		
		if (ElapsedTime >= ShrinkDuration)
		{
			CurrRadius = DestRadius;
			CurrCenter = DestCenter;
			PrevRadius = CurrRadius;
			PrevCenter = CurrCenter;
			bIsShrinking = false;
			++NumShrinks;
			
			UE_LOG(LogTemp, Log, TEXT("Shrinking %d%%"), NumShrinks);
		}
	}
}

void UTimeStormComponent::StartShrinkingStormZone(const FVector2D& InDestCenter, float InDestRadius,
	float InShrinkDuration)
{
	if (InShrinkDuration > 0)
	{
		ShrinkDuration = InShrinkDuration;
	}
	
	DestRadius = InDestRadius;
	DestCenter = InDestCenter;
	
	ElapsedTime = 0.0f;
	bIsShrinking = true;
}

void UTimeStormComponent::StartRandomStormZoneShrink(float InShrinkDuration)
{
	float NextRadius; 
	
	if (NumShrinks == 0)
	{
		NextRadius = MapSize.X / 2.0f * 5.0f / 6.0f;
	}
	else
	{
		NextRadius = PrevRadius / 2.0f;
	}
	
	float Dist = FMath::RandRange(0.0f, FMath::Min(PrevRadius, MapSize.X / 2) - NextRadius);
	
	FVector2D RandomCenter = PrevCenter + FMath::RandPointInCircle(Dist);
	
	StartShrinkingStormZone(RandomCenter, NextRadius, InShrinkDuration);
}

void UTimeStormComponent::GetCurrStormZone_UV(FVector2D& OutCenter, float& OutRadius) const
{
	float MaxSize = MapSize.X;
	OutCenter.X = CurrCenter.Y / MaxSize + 0.5f;
	OutCenter.Y = ( MaxSize / 2 - CurrCenter.X ) / MaxSize;
	OutRadius = CurrRadius / MaxSize;
}

void UTimeStormComponent::GetDestStormZone_UV(FVector2D& OutCenter, float& OutRadius) const
{
	float MaxSize = MapSize.X;
	OutCenter.X = DestCenter.Y / MaxSize + 0.5f;
	OutCenter.Y = ( MaxSize / 2 - DestCenter.X ) / MaxSize;
	OutRadius = DestRadius / MaxSize;
}

