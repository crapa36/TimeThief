// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ItemPoolWorldSubsystem.generated.h"

/**
 * 
 */
constexpr int InitPoolSize = 50;

USTRUCT()
struct FActorPool
{
	GENERATED_BODY();
	
	FActorPool()
	{
		Actors.Reserve(InitPoolSize);
	}
	
	UPROPERTY()
	TArray<TObjectPtr<AActor>> Actors;
};

UCLASS()
class TIMETHIEF_API UItemPoolWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void OnWorldEndPlay(UWorld& InWorld) override;
	
	UFUNCTION(BlueprintCallable, Category = "TimeThief|ItemPool")
	AActor* Get(TSubclassOf<AActor> ObjectClass);

private:
	UPROPERTY()
	TMap<TSubclassOf<AActor>, FActorPool> Pools;
};
