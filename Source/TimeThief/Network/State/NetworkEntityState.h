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
	float Yaw = 0.0f;
	float Pitch = 0.0f;
	float Hp = 0.0f;
	bool bSpawned = false;
};