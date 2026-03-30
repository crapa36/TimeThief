#include "Game/TimeThiefGameMode.h"
#include "Character/TimeThiefPlayerCharacter.h"
#include "Character/TimeThiefPawnData.h"
#include "GameFramework/PlayerController.h"

ATimeThiefGameMode::ATimeThiefGameMode() {
}

UClass* ATimeThiefGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
    UE_LOG(LogTemp, Warning, TEXT("[SpawnTrace] GetDefaultPawnClassForController FINAL Result=None"));
    return nullptr;
}

void ATimeThiefGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) {
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	
	if (NewPlayer && DefaultPawnData) {
		if (ATimeThiefPlayerCharacter* PlayerCharacter = Cast<ATimeThiefPlayerCharacter>(NewPlayer->GetPawn())) {
			PlayerCharacter->SetPawnData(DefaultPawnData);
		}
	}
}

void ATimeThiefGameMode::RestartPlayer(AController* NewPlayer)
{
	Super::RestartPlayer(NewPlayer);

	if (NewPlayer && DefaultPawnData)
	{
		if (ATimeThiefPlayerCharacter* PlayerCharacter = Cast<ATimeThiefPlayerCharacter>(NewPlayer->GetPawn()))
		{
			PlayerCharacter->SetPawnData(DefaultPawnData);
		}
	}
}