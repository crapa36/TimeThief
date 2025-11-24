#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TimeThiefGameMode.generated.h"

/**
 * 게임의 규칙을 총괄하는 게임 모드 클래스
 */
UCLASS(minimalapi)
class ATimeThiefGameMode : public AGameModeBase {
	GENERATED_BODY()

public:
	ATimeThiefGameMode();
};