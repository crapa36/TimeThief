#include "Game/TimeThiefGameMode.h"
#include "Character/TimeThiefPlayerCharacter.h"
#include "Character/TimeThiefPawnData.h"
#include "GameFramework/PlayerController.h"

ATimeThiefGameMode::ATimeThiefGameMode() {
}

UClass* ATimeThiefGameMode::GetDefaultPawnClassForController_Implementation(AController* InController) {
	if (DefaultPawnData && DefaultPawnData->PawnClass) {
		return DefaultPawnData->PawnClass;
	}
	return ATimeThiefPlayerCharacter::StaticClass();
}

void ATimeThiefGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) {
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	
	if (NewPlayer && DefaultPawnData) {
		if (ATimeThiefPlayerCharacter* PlayerCharacter = Cast<ATimeThiefPlayerCharacter>(NewPlayer->GetPawn())) {
			PlayerCharacter->SetPawnData(DefaultPawnData);
		}
	}
}

