#pragma once

#include "CoreMinimal.h"
#include "LocalPlayerInfo.generated.h"

USTRUCT()
struct FLocalPlayerInfo
{
	GENERATED_BODY()
	
	UPROPERTY()
	uint64 PlayerId = 0;
	
	UPROPERTY()
	FString Nickname;
	
};
