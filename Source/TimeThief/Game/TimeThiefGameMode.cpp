#include "Game/TimeThiefGameMode.h"
#include "Character/TimeThiefPlayerCharacter.h" 
#include "UObject/ConstructorHelpers.h"

ATimeThiefGameMode::ATimeThiefGameMode() {

	DefaultPawnClass = ATimeThiefPlayerCharacter::StaticClass();
}