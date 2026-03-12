#pragma once

#include "CoreMinimal.h"
#include "NetworkEntityState.generated.h"

USTRUCT()
struct FNetworkEntityState
{
	GENERATED_BODY()
	
	uint32 EntityId;
	FVector Position = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	float Hp = 0.0f;
	bool bSpawned = false;
};