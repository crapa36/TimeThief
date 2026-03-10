#pragma once

#include "CoreMinimal.h"
#include "RoomPlayerInfo.h"

USTRUCT(BlueprintType)
struct FRoomState
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly)
	uint64 RoomId = 0;
	
	UPROPERTY(BlueprintReadOnly)
	TArray<FRoomPlayerInfo> RoomStates;
	
};
