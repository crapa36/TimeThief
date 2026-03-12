#pragma once

#include "CoreMinimal.h"
#include "RoomPlayerInfo.generated.h"

USTRUCT()
struct FRoomPlayerInfo
{
	GENERATED_BODY()
	
	UPROPERTY()
	uint64 PlayerId = 0;
	
	UPROPERTY()
	uint32 EntityId = 0;
	
	UPROPERTY()
	FString Nickname;
	
	UPROPERTY()
	bool bReady = false;
	
};
