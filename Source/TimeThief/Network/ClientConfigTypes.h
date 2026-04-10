#pragma once

#include "CoreMinimal.h"

#include "ClientConfigTypes.generated.h"

USTRUCT(BlueprintType)
struct FClientConfig
{
	GENERATED_BODY()
	
public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FString ServerDNS = TEXT("localhost");
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FString FallbackIp = TEXT("127.0.0.1");
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 ServerPort = 8252;
	
};
