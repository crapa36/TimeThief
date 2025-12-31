#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TimeThiefGameMode.generated.h"

class UTimeThiefExperienceDefinition;
class UTimeThiefPawnData;

UCLASS(minimalapi)
class ATimeThiefGameMode : public AGameModeBase {
	GENERATED_BODY()
public:
	ATimeThiefGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "TimeThief|Experience")
	TObjectPtr<UTimeThiefExperienceDefinition> DefaultExperience;

	UPROPERTY(EditDefaultsOnly, Category = "TimeThief|Pawn")
	TObjectPtr<UTimeThiefPawnData> DefaultPawnData;

	void OnExperienceLoaded(const UTimeThiefExperienceDefinition* Experience);
};
