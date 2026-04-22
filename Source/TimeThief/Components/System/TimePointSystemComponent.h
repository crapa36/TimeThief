// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/Interface/LifeObserver.h"
#include "TimePointSystemComponent.generated.h"


DECLARE_MULTICAST_DELEGATE_OneParam(FOnTimePointsChanged, int /*NewTimePoints*/);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class TIMETHIEF_API UTimePointSystemComponent : public UActorComponent, public ILifeObserver
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UTimePointSystemComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
	
	virtual void OnEndRespawn() override;
	
	virtual void OnDeath() override;
	
	bool ModifyTimePoints(int Value);
	void SetTimePoints(int Value);
	bool UpdateTimePoints(int NewValue, int Delta);
	
	int GetTimePoints() const { return static_cast<int>(TimePoints); }
	
	FOnTimePointsChanged OnTimePointsChanged_Delegate;
private:
	UPROPERTY(EditAnywhere, meta=(AllowPrivateAccess="true"), Category="Level Design")
	float TimePointsGainPerSecond = 10.0f;
	
	UPROPERTY(EditAnywhere, meta=(AllowPrivateAccess="true"), Category="Level Design")
	float DangerThreshold = 1000;
	
	float DamagedElapsedTime = 0.0f;
	float RecoveredElapsedTime = 0.0f;
	
	float TimePoints = 100000.0f;
	
	int LastDisplayTimePoints = TimePoints;
	
	void HandleTimePointsChanged(int InTimePoints);
};
