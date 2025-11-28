#pragma once

#include "CoreMinimal.h"
#include "Character/TimeThiefCharacterBase.h"
#include "TimeThiefEnemyCharacter.generated.h"

UCLASS()
class TIMETHIEF_API ATimeThiefEnemyCharacter : public ATimeThiefCharacterBase {
	GENERATED_BODY()

public:
	ATimeThiefEnemyCharacter();

protected:
	virtual void BeginPlay() override;
	virtual void InitAbilityActorInfo() override;
};