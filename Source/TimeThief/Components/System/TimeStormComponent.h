// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimeStormComponent.generated.h"


class UTimeStormData;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class TIMETHIEF_API UTimeStormComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UTimeStormComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	
	virtual void OnRegister() override;
public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
	
	UFUNCTION(BlueprintCallable)
	void ReStart();
	
	UFUNCTION(BlueprintCallable)
	void StartShrinkingStormZone(const FVector2D& InDestCenter, float InDestRadius);
	
	UFUNCTION(BlueprintCallable)
	void StartRandomStormZoneShrink();
	
	void GetCurrStormZone_UV(FVector2D& OutCenter, float& OutRadius) const;
	void GetDestStormZone_UV(FVector2D& OutCenter, float& OutRadius) const;
	
	void GetCurrStormZone(FVector2D& OutCenter, float& OutRadius) const;
	
	UPROPERTY(EditAnywhere)
	FVector2f MapSize;
	
private:
	UPROPERTY(EditAnywhere, meta=(AllowPrivateAccess="true"))
	TObjectPtr<UTimeStormData> DataTable;
	
	float ElapsedTime = 0.0f;
	float ShrinkDuration = 0.5f;
	
	FVector2D CurrCenter{0.0f, 0.0f};
	FVector2D PrevCenter{0.0f, 0.0f};
	FVector2D DestCenter{0.0f, 0.0f};
	
	float PrevRadius;
	float CurrRadius;
	float DestRadius;
	
	bool bIsShrinking = false;
	
	int NumShrinks = 0;
};
