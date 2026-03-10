#pragma once

#include "CoreMinimal.h"

USTRUCT(BlueprintType)
struct FLocalPlayerInfo
{
	GENERATED_BODY()
	
	UPROPERTY()
	uint64 PlayerId = 0;
	
	UPROPERTY(BlueprintReadOnly)
	FString Nickname;
	
};
