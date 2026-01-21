#pragma once

#include "CoreMinimal.h"
#include "TimeThiefWireTypes.generated.h"

UENUM(BlueprintType)
enum class EWireState : uint8
{
	Idle,
	Firing,
	Attached
};
