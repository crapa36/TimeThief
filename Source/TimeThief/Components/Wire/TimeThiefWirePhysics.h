#pragma once

#include "CoreMinimal.h"
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
	float SpringStiffness = 5000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Physics")
	float SpringDamping = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Physics")
	float PullForce = 400000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Physics")
	float SwingInputForce = 150000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Physics")
	float WireResistance = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Physics")
	float MaxSwingSpeedMultiplier = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Physics")
	float VerticalDamping = 75000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Physics")
	float MaxGroundTime = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Physics")
	float WireBreakSpeedThreshold = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Physics")
	float WireBreakAngleThreshold = -0.5f;

protected:

	UPROPERTY(Transient)
	TObjectPtr<UCharacterMovementComponent> CachedMovementComponent;
};
