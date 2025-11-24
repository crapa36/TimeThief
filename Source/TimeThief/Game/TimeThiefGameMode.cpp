#include "Game/TimeThiefGameMode.h"
#include "Character/TimeThiefPlayerController.h"
#include "Character/TimeThiefCharacter.h"
#include "UObject/ConstructorHelpers.h"

ATimeThiefGameMode::ATimeThiefGameMode() {
	// 기본 플레이어 컨트롤러 설정
	PlayerControllerClass = ATimeThiefPlayerController::StaticClass();

}