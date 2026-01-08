#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TimeThiefCharacterBase.generated.h"

class UTimeThiefPawnCombatComponent;

UCLASS()
class TIMETHIEF_API ATimeThiefCharacterBase : public ACharacter
{
	GENERATED_BODY()

public:
	ATimeThiefCharacterBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Combat")
	virtual UTimeThiefPawnCombatComponent* GetPawnCombatComponent() const { return nullptr; }
};