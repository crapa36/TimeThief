// Fill out your copyright notice in the Description page of Project Settings.


#include "TimeThiefGameState.h"

#include "Components/System/TimeStormComponent.h"

ATimeThiefGameState::ATimeThiefGameState()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	
	TimeStormComponent = CreateDefaultSubobject<UTimeStormComponent>(TEXT("TimeStormComponent"));
}


