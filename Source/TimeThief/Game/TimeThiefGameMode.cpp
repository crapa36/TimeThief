#include "Game/TimeThiefGameMode.h"
#include "Character/TimeThiefPlayerCharacter.h" 
#include "Character/TimeThiefPawnData.h"
#include "Game/TimeThiefExperienceDefinition.h"
#include "GameFramework/PlayerController.h"

ATimeThiefGameMode::ATimeThiefGameMode() {
	DefaultPawnClass = ATimeThiefPlayerCharacter::StaticClass();
}

void ATimeThiefGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) {
	Super::InitGame(MapName, Options, ErrorMessage);

	if (DefaultExperience && DefaultExperience->DefaultPawnData) {
		DefaultPawnData = DefaultExperience->DefaultPawnData;
	}
}

void ATimeThiefGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) {
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	
	if (NewPlayer && DefaultPawnData) {
		if (ATimeThiefPlayerCharacter* PlayerCharacter = Cast<ATimeThiefPlayerCharacter>(NewPlayer->GetPawn())) {
			PlayerCharacter->SetPawnData(DefaultPawnData);
		}
	}
}

void ATimeThiefGameMode::OnExperienceLoaded(const UTimeThiefExperienceDefinition* Experience) {
	if (Experience && Experience->DefaultPawnData) {
		DefaultPawnData = Experience->DefaultPawnData;
	}
}
