#pragma once

#include "CoreMinimal.h"
#include "CombatNotifyType.generated.h"

UENUM(BlueprintType)
enum ECombatNotifyType : uint8
{
	None,
	Fire,
	Throw,
	Reload,
	Hit,
};
