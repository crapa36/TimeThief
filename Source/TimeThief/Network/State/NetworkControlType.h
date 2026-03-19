#pragma once

#include "CoreMinimal.h"

#include "NetworkControlType.generated.h"

UENUM(BlueprintType)
enum class ENetworkControlType : uint8
{
	None			UMETA(DisplayName = "None"),
	Local			UMETA(DisplayName = "Local"),
	Remote			UMETA(DisplayName = "Remote"),
	ServerAuth		UMETA(DisplayName = "ServerAuth"),
};
