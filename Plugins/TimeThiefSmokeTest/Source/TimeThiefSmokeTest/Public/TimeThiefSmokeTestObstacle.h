#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimeThiefSmokeTestObstacle.generated.h"

class UBoxComponent;
class UCapsuleComponent;
class USphereComponent;

struct FTimeThiefSmokeTestObstacleSettings
{
	FString Shape;
	FVector Position = FVector::ZeroVector;
	FVector Extent = FVector(50.0);
	float Radius = 50.0f;
	float HalfHeight = 100.0f;
};

UCLASS()
class TIMETHIEFSMOKETEST_API ATimeThiefSmokeTestObstacle : public AActor
{
	GENERATED_BODY()

public:
	ATimeThiefSmokeTestObstacle();
	void Configure(const FTimeThiefSmokeTestObstacleSettings& Settings);

private:
	UPROPERTY()
	TObjectPtr<UBoxComponent> BoxComponent;

	UPROPERTY()
	TObjectPtr<UCapsuleComponent> CapsuleComponent;

	UPROPERTY()
	TObjectPtr<USphereComponent> SphereComponent;
};
