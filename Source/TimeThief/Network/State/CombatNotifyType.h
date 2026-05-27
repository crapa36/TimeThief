#pragma once

#include "CoreMinimal.h"
#include "CombatNotifyType.generated.h"

UENUM(BlueprintType)
enum class ECombatNotifyType : uint8
{
	None			UMETA(DisplayName = "None"),				// 기본값, 유효하지 않은 상태를 나타냅니다.
	Attack			UMETA(DisplayName = "Attack"),				// 일반 공격을 나타냅니다. (Monster)
	Cancel			UMETA(DisplayName = "Cancel"),				// 공격 취소를 나타냅니다. (Monster)
	Fire			UMETA(DisplayName = "Fire"),				// 총격을 나타냅니다. (Player/Monster)
	Throw			UMETA(DisplayName = "Throw"),				// 수류탄 투척을 나타냅니다. (Player)
	WeaponChange	UMETA(DisplayName = "Weapon Change"),		// 무기 변경을 나타냅니다. (Player)
	Aiming			UMETA(DisplayName = "Aiming"),				// 조준을 나타냅니다. (Player)
	Readying		UMETA(DisplayName = "Readying"),			// 조준 해제를 나타냅니다. (Player)
	Reload			UMETA(DisplayName = "Reload"),				// 재장전을 나타냅니다. (Player)
	Hit				UMETA(DisplayName = "Hit"),					// 피격을 나타냅니다. (Player/Monster)
};
