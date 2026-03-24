#pragma once

#include "CoreMinimal.h"
#include "RuntimeConfig.generated.h"

USTRUCT(BlueprintType)
struct FRuntimeConfig
{
	GENERATED_BODY()
	
public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 MovementUpdateHz = 10;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 PingIntervalMs = 1000;
	
public:
	float GetMovementUpdateIntervalSeconds() const
	{
		return MovementUpdateHz > 0 ? static_cast<float>(1.0f / MovementUpdateHz) : 1.0f;
	}
	
	float GetPingIntervalSeconds() const
	{
		return static_cast<float>(PingIntervalMs) / 1000.0f;
	}
	
	bool IsValid() const
	{
		return ((MovementUpdateHz > 0) && (PingIntervalMs > 0));
	}
};
