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
	
	PhaseElapsedTime = 0.0f;
	WaitDuration = 0.0f;
	ShrinkDuration = 0.5f;
	bIsShrinking = false;
	NumShrinks = 0;
	
	// if (DataTable)
	// {
	// 	ShrinkDuration = DataTable->TimeStormShrinkTimePerLevel[0];
	// }
}


// Called every frame
void UTimeStormComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                        FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	if (DeltaTime <= 0.0f)
	{
		return;
	}

	PhaseElapsedTime += DeltaTime;

	if (bIsShrinking == false)
	{
		// 서버 ZoneSystem의 대기 단계와 동일
		if (PhaseElapsedTime >= WaitDuration)
		{
			PhaseElapsedTime -= WaitDuration;
			bIsShrinking = true;
		}
	}
	else
	{
		// 서버 ZoneSystem의 수축 단계와 동일
		const float SafeShrinkDuration = FMath::Max(ShrinkDuration, KINDA_SMALL_NUMBER);
		const float Alpha = FMath::Clamp(PhaseElapsedTime / SafeShrinkDuration, 0.0f, 1.0f);

		CurrCenter = FMath::Lerp(PrevCenter, DestCenter, Alpha);
		CurrRadius = FMath::Lerp(PrevRadius, DestRadius, Alpha);

		if (PhaseElapsedTime >= ShrinkDuration)
		{
			CurrCenter = DestCenter;
			CurrRadius = DestRadius;

			PrevCenter = CurrCenter;
			PrevRadius = CurrRadius;

			PhaseElapsedTime = 0.0f;
			bIsShrinking = false;
			++NumShrinks;
		}
	}
	
	// 기존 코드 (Data Table 안쓸거라 일단 주석)
	// if (!DataTable || NumShrinks >= DataTable->TimeStormShrinkTimePerLevel.Num())
	// {
	// 	return;
	// }
	//
	// ElapsedTime += DeltaTime;
	//
	// if (bIsShrinking)
	// {
	// 	float Alpha = FMath::Min(ElapsedTime / ShrinkDuration, 1.0f);
	// 	
	// 	CurrRadius = FMath::Lerp(PrevRadius, DestRadius, Alpha);
	// 	CurrCenter = FMath::Lerp(PrevCenter, DestCenter, Alpha);
	// 	
	// 	if (ElapsedTime >= ShrinkDuration)
	// 	{
	// 		++NumShrinks;
	// 		bIsShrinking = false;
	// 		ElapsedTime = 0;
	// 	}
	// }
	// else if (ElapsedTime >= DataTable->TimeStormStartTimePerLevel[NumShrinks])
	// {
	// 	StartRandomStormZoneShrink();
	// 	ElapsedTime = 0;
	// }
}

void UTimeStormComponent::ReStart()
{
	PhaseElapsedTime = 0.0f;
	WaitDuration = 0.0f;
	ShrinkDuration = 0.5f;

	NumShrinks = 0;

	CurrCenter = FVector2D{0.0f, 0.0f};
	PrevCenter = FVector2D{0.0f, 0.0f};
	DestCenter = FVector2D{0.0f, 0.0f};

	DestRadius = MapSize.X * 1.5f;
	CurrRadius = DestRadius;
	PrevRadius = DestRadius;

	bIsShrinking = false;
	
	// 기존 코드 (Restart가 없어야 하지 않나 싶지만 일단 남겨둠)
	// ElapsedTime = 0.0f;
	// NumShrinks = 0;
	// CurrCenter = {0.0f, 0.0f};
	// PrevCenter = {0.0f, 0.0f};
	// DestCenter = {0.0f, 0.0f};
	// bIsShrinking = false;
}

void UTimeStormComponent::StartShrinkingStormZone(const FVector2D& InDestCenter, float InDestRadius)
{
	DestRadius = InDestRadius;
	DestCenter = InDestCenter;
	
	// ElapsedTime = 0.0f;
	// ElapsedTime 이름 변경하면서 기존 코드에서 PhaseElapsedTime으로 변경
	PhaseElapsedTime = 0.0f;
	bIsShrinking = true;
}

void UTimeStormComponent::StartRandomStormZoneShrink()
{
	// 기존 코드 (Data Table 안쓸거라 일단 주석, 그리고 다음 자기장 판단은 서버에서 할 거라 이 부분도 없어질 듯)
	// if (NumShrinks >= DataTable->TimeStormShrinkTimePerLevel.Num())
	// {
	// 	return;
	// }
	//
	// CurrRadius = DestRadius;
	// CurrCenter = DestCenter;
	//
	// PrevRadius = CurrRadius;
	// PrevCenter = CurrCenter;
	//
	// ShrinkDuration = DataTable->TimeStormShrinkTimePerLevel[NumShrinks];
	//
	// float NextRadius = DataTable->TimeStormRadiusPerLevel[NumShrinks];
	//
	// float Dist = FMath::RandRange(0.0f, FMath::Min(PrevRadius, MapSize.X / 2) - NextRadius);
	//
	// FVector2D RandomCenter = PrevCenter + FMath::RandPointInCircle(Dist);
	//
	// StartShrinkingStormZone(RandomCenter, NextRadius);
}

void UTimeStormComponent::SetStormPhase(const FVector2D& InDestCenter, float InDestRadius, float InWaitingTime,
	float InShrinkingTime)
{
	PrevCenter = CurrCenter;
	PrevRadius = CurrRadius;
	
	DestCenter = InDestCenter;
	DestRadius = InDestRadius;
	
	WaitDuration = FMath::Max(0.0f, InWaitingTime);
	ShrinkDuration = FMath::Max(0.0f, InShrinkingTime);
	
	PhaseElapsedTime = 0.0f;
	bIsShrinking = false;
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

