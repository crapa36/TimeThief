#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "TimeThiefCharacterBase.generated.h"

class UTimeThiefPawnCombatComponent;

UCLASS()
class TIMETHIEF_API ATimeThiefCharacterBase : public ACharacter {
	GENERATED_BODY()

public:
	ATimeThiefCharacterBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Combat")
	virtual UTimeThiefPawnCombatComponent* GetPawnCombatComponent() const { return nullptr; }

protected:
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
};