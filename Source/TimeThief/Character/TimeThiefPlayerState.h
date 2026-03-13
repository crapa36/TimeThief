// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "TimeThiefPlayerState.generated.h"

/**
 * 
 */
USTRUCT(BlueprintType)
struct FStatus
{
	GENERATED_BODY()
	
	int Damage = 0;	
	int Stability = 0;
	int Capacity = 0;
	int Health = 0;
	int Speed = 0;
};

UCLASS()
class TIMETHIEF_API ATimeThiefPlayerState : public APlayerState
{
	GENERATED_BODY()
	
public:
	FStatus Status;
};
