#pragma once

#include "CoreMinimal.h"
#include "RoomPlayerInfo.h"

#include "RoomState.generated.h"

USTRUCT()
struct FRoomState
{
	GENERATED_BODY()
	
	UPROPERTY()
	uint32 RoomId = 0;
	
	UPROPERTY()
	TArray<FRoomPlayerInfo> RoomStates;
	
};
