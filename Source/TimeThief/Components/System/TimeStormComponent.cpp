// Fill out your copyright notice in the Description page of Project Settings.


#include "TimeStormComponent.h"

#include "DataAssets/TimeStormData.h"


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
	
	
	DestCenter = FVector2D{0.0f};
	DestRadius = MapSize.X * 1.5f;
	
	CurrRadius = DestRadius;
	CurrCenter = DestCenter;
	
	PrevRadius = CurrRadius;
	PrevCenter = CurrCenter;
	if (DataTable)
	{
		ShrinkDuration = DataTable->TimeStormShrinkTimePerLevel[0];
	}
}


// Called every frame
void UTimeStormComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                        FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	if (!DataTable || NumShrinks >= DataTable->TimeStormShrinkTimePerLevel.Num())
	{
		return;
	}
	
	ElapsedTime += DeltaTime;
	
	if (bIsShrinking)
	{
		float Alpha = FMath::Min(ElapsedTime / ShrinkDuration, 1.0f);
		
		CurrRadius = FMath::Lerp(PrevRadius, DestRadius, Alpha);
		CurrCenter = FMath::Lerp(PrevCenter, DestCenter, Alpha);
		
		if (ElapsedTime >= ShrinkDuration)
		{
			++NumShrinks;
			bIsShrinking = false;
			ElapsedTime = 0;
		}
	}
	else if (ElapsedTime >= DataTable->TimeStormStartTimePerLevel[NumShrinks])
	{
		StartRandomStormZoneShrink();
		ElapsedTime = 0;
	}
}

void UTimeStormComponent::ReStart()
{
	ElapsedTime = 0.0f;
	NumShrinks = 0;
	bIsShrinking = false;
}

void UTimeStormComponent::StartShrinkingStormZone(const FVector2D& InDestCenter, float InDestRadius)
{
	DestRadius = InDestRadius;
	DestCenter = InDestCenter;
	
	ElapsedTime = 0.0f;
	bIsShrinking = true;
}

void UTimeStormComponent::StartRandomStormZoneShrink()
{
	if (NumShrinks >= DataTable->TimeStormShrinkTimePerLevel.Num())
	{
		return;
	}
	
	CurrRadius = DestRadius;
	CurrCenter = DestCenter;
	
	PrevRadius = CurrRadius;
	PrevCenter = CurrCenter;
	
	ShrinkDuration = DataTable->TimeStormShrinkTimePerLevel[NumShrinks];
	
	float NextRadius = DataTable->TimeStormRadiusPerLevel[NumShrinks];
	
	float Dist = FMath::RandRange(0.0f, FMath::Min(PrevRadius, MapSize.X / 2) - NextRadius);
	
	FVector2D RandomCenter = PrevCenter + FMath::RandPointInCircle(Dist);
	
	StartShrinkingStormZone(RandomCenter, NextRadius);
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

void UTimeStormComponent::GetCurrStormZone(FVector2D& OutCenter, float& OutRadius) const
{
	OutCenter = CurrCenter;
	OutRadius = CurrRadius;
}

