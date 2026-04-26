#pragma once

#include "CoreMinimal.h"
#include "Protocol.pb.h"

#include "NetworkEntityState.generated.h"

USTRUCT()
struct FNetworkEntityState
{
	GENERATED_BODY()
	
	uint32 EntityId;
	se::common::ObjectType ObjectType;
	uint32 TemplateId = 0;
	
	FVector Position = FVector::ZeroVector;
	float CharYaw = 0.0f;
	float AimYaw = 0.0f;	// Player Only
	float AimPitch = 0.0f;
	FVector Velocity = FVector::ZeroVector;
	EMovementMode MovementMode = EMovementMode::MOVE_None;
	
	float Hp = 0.0f;
	bool bSpawned = false;
};