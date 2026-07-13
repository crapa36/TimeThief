#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimeThiefSmokeTestMover.generated.h"

class UBoxComponent;
class UCapsuleComponent;
class USphereComponent;

struct FTimeThiefSmokeTestMoverSettings
{
	FString Shape;
	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
	FVector Extent = FVector(50.0);
	float Duration = 1.0f;
	float Radius = 50.0f;
	float HalfHeight = 100.0f;
};

UCLASS()
class TIMETHIEFSMOKETEST_API ATimeThiefSmokeTestMover : public AActor
{
	GENERATED_BODY()

public:
	ATimeThiefSmokeTestMover();
	void Configure(const FTimeThiefSmokeTestMoverSettings& Settings);
	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY()
	TObjectPtr<UBoxComponent> BoxComponent;

	UPROPERTY()
	TObjectPtr<UCapsuleComponent> CapsuleComponent;

	UPROPERTY()
	TObjectPtr<USphereComponent> SphereComponent;

	FVector StartPosition = FVector::ZeroVector;
	FVector EndPosition = FVector::ZeroVector;
	float StartTime = 0.0f;
	float Duration = 1.0f;
};
