// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TimeStormData.generated.h"

/**
 * 
 */
UCLASS()
class TIMETHIEF_API UTimeStormData : public UDataAsset
{
	GENERATED_BODY()
	
public:
	UTimeStormData();
	
	UPROPERTY(EditAnywhere, EditFixedSize)
	TArray<float> TimeStormStartTimePerLevel;
	
	UPROPERTY(EditAnywhere, EditFixedSize)
	TArray<float> TimeStormShrinkTimePerLevel;
	
	UPROPERTY(EditAnywhere, EditFixedSize)
	TArray<float> TimeStormRadiusPerLevel;
};
