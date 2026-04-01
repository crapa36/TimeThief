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
	float Pitch = 0.0f;
	
	UPROPERTY()
	FVector2D Velocity = FVector2D::ZeroVector;
	
	EMovementMode MovementMode = EMovementMode::MOVE_None;
	
};
