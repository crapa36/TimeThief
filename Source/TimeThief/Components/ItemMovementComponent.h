#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ItemMovementComponent.generated.h"

class USceneComponent;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class TIMETHIEF_API UItemMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UItemMovementComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category="Item Movement")
	void ResetMovementOrigin();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Item Movement")
	TObjectPtr<USceneComponent> TargetComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Item Movement", meta=(ClampMin="0.0"))
	float RotationSpeedDegrees = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Item Movement", meta=(ClampMin="0.0"))
	float BobAmplitude = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Item Movement", meta=(ClampMin="0.0"))
	float BobFrequency = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Item Movement")
	bool bRandomizeStartPhase = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Item Movement")
	bool bSkipWhenTargetHidden = true;

private:
	USceneComponent* ResolveTargetComponent() const;
	bool IsTargetRootComponent(const USceneComponent* Target) const;
	bool WasMovedExternally(const USceneComponent* Target) const;

	FVector InitialRelativeLocation = FVector::ZeroVector;
	FRotator InitialRelativeRotation = FRotator::ZeroRotator;
	FVector InitialWorldLocation = FVector::ZeroVector;
	FRotator InitialWorldRotation = FRotator::ZeroRotator;
	FVector LastAppliedWorldLocation = FVector::ZeroVector;
	FRotator LastAppliedWorldRotation = FRotator::ZeroRotator;
	float ElapsedTime = 0.0f;
	bool bHasOrigin = false;
	bool bHasAppliedWorldTransform = false;
};
