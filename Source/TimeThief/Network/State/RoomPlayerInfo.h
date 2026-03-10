#pragma once

#include "CoreMinimal.h"

USTRUCT(BlueprintType)
struct FRoomPlayerInfo
{
	GENERATED_BODY()
	
	UPROPERTY()
	uint64 PlayerId = 0;
	
	UPROPERTY()
	uint32 EntityId = 0;
	
	UPROPERTY(BlueprintReadOnly)
	FString Nickname;
	
	UPROPERTY(BlueprintReadOnly)
	bool bReady = false;
	
};
