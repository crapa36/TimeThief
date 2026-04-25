#pragma once

#include "CoreMinimal.h"
#include "MoveSyncData.generated.h"

USTRUCT(BlueprintType)
struct FMoveSyncData
{
	GENERATED_BODY()
	
	UPROPERTY()
	FVector Position = FVector::ZeroVector;
	
	UPROPERTY()
	float Yaw = 0.0f;
	
	UPROPERTY()
	float AimYaw = 0.0f;
	
	UPROPERTY()
	float Pitch = 0.0f;
	
	UPROPERTY()
	FVector Velocity = FVector::ZeroVector;
	
	EMovementMode MovementMode = EMovementMode::MOVE_None;
	
};
