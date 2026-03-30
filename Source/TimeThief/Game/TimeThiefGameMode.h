#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TimeThiefGameMode.generated.h"

class UTimeThiefPawnData;

UCLASS(minimalapi)
class ATimeThiefGameMode : public AGameModeBase {
	GENERATED_BODY()
public:
	ATimeThiefGameMode();

	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
	virtual void RestartPlayer(AController* NewPlayer) override;

	const UTimeThiefPawnData* GetDefaultPawnData() const { return DefaultPawnData; }

protected:
	UPROPERTY(EditDefaultsOnly, Category = "TimeThief|Pawn")
	TObjectPtr<UTimeThiefPawnData> DefaultPawnData;
};