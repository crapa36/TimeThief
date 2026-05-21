#pragma once

#include "CoreMinimal.h"
#include "CombatNotifyType.generated.h"

UENUM(BlueprintType)
enum class ECombatNotifyType : uint8
{
	None,
	Fire,
	Throw,
	WeaponChange,
	Aiming,			// 조준
	Readying,		// 조준 해제
	Reload,
	Hit,
};
