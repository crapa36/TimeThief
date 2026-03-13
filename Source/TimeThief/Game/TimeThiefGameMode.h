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

protected:
	UPROPERTY(EditDefaultsOnly, Category = "TimeThief|Pawn")
	TObjectPtr<UTimeThiefPawnData> DefaultPawnData;
};
