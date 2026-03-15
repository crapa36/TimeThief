// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "TimeThiefGameState.generated.h"

class UTimeStormComponent;
/**
 * 
 */
UCLASS()
class TIMETHIEF_API ATimeThiefGameState : public AGameStateBase
{
	GENERATED_BODY()
	
public:
	ATimeThiefGameState();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UTimeStormComponent> TimeStormComponent;
};
