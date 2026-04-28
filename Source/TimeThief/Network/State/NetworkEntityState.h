#pragma once

#include "CoreMinimal.h"
#include "Protocol.pb.h"

#include "NetworkEntityState.generated.h"

USTRUCT()
struct FNetworkEntityState
{
	GENERATED_BODY()
	
	uint32 EntityId;
	se::common::ObjectType ObjectType = se::common::ObjectType::OBJ_NONE;
	uint32 TemplateId = 0;
	
	FVector Position = FVector::ZeroVector;
	float CharYaw = 0.0f;
	float AimYaw = 0.0f;	// Player Only
	float AimPitch = 0.0f;
	FVector Velocity = FVector::ZeroVector;
	EMovementMode MovementMode = EMovementMode::MOVE_None;
	
	uint32 ItemCount = 0;
	
	float Hp = 0.0f;
	bool bSpawned = false;
};