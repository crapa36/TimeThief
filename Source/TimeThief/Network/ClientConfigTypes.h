#pragma once

#include "CoreMinimal.h"

#include "ClientConfigTypes.generated.h"

USTRUCT(BlueprintType)
struct FClientConfig
{
	GENERATED_BODY()
	
public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FString ServerIp = TEXT("127.0.0.1");
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 ServerPort = 8252;
	
};
