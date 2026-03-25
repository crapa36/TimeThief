#pragma once

#include "CoreMinimal.h"
#include "NetworkEntityState.h"
#include "EntityRuntimeEntry.generated.h"

USTRUCT()
struct FEntityRuntimeEntry
{
	GENERATED_BODY()
	
	UPROPERTY()
	FNetworkEntityState State;
	
	TWeakObjectPtr<AActor> Actor;
	
	uint32 EntityId = 0;
	// bool bIsSpawned = false;
	// bool bInitialized = false;
	// bool bPendingDestroy = false;
};
