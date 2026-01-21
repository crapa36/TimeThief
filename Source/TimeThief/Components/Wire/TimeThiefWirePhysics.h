#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "TimeThiefWirePhysics.generated.h"

class UCharacterMovementComponent;

UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class TIMETHIEF_API UTimeThiefWirePhysics : public UObject
{
	GENERATED_BODY()

public:
	virtual void Initialize(UCharacterMovementComponent* InMovementComponent);
	virtual void ApplyWirePhysics(float DeltaTime, const FVector& AnchorPoint, const FVector& WireStartLocation, float WireLength, const FVector2D& Input);

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Physics")
	float PullInForce = 150000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Physics")
	float SwingInputForce = 150000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Physics")
	float CentrifugalMassMultiplier = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Physics")
	float MaxSwingSpeedMultiplier = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Physics")
	float GravityMultiplierOnWire = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Physics")
	float SwingDragCoefficient = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Physics")
	float MinWireLengthForPhysics = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Physics")
	float MaxGroundTime = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Physics")
	float WireBreakSpeedThreshold = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Physics")
	float WireBreakAngleThreshold = -0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Physics|Stabilization")
	float VerticalDampingLow = 0.98f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Physics|Stabilization")
	float VerticalDampingHigh = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Physics|Stabilization")
	float PositionCorrectionSpeed = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Physics|Stabilization")
	float AnchorHeightThreshold = 200.0f;

protected:
	virtual FVector CalculatePullForce(const FVector& WireDirection, float CurrentDistance) const;
	virtual FVector CalculateSwingInputForce(const FVector& WireDirection, const FVector2D& Input) const;
	virtual FVector GetTangentVelocity(const FVector& Velocity, const FVector& WireDirection) const;

	UPROPERTY(Transient)
	TObjectPtr<UCharacterMovementComponent> CachedMovementComponent;
};
