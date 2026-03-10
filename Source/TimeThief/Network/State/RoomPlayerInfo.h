#pragma once

#include "CoreMinimal.h"

USTRUCT(BlueprintType)
struct FRoomPlayerInfo
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly)
	uint64 PlayerId = 0;
	
	UPROPERTY(BlueprintReadOnly)
	uint64 EntityId = 0;
	
	UPROPERTY(BlueprintReadOnly)
	FString Nickname;
	
	UPROPERTY(BlueprintReadOnly)
	bool bReady = false;
	
};
