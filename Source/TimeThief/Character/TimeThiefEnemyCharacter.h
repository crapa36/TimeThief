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

	UFUNCTION()
	void OnDeath(AActor* OwningActor);

	UPROPERTY(EditDefaultsOnly, Category = "TimeThief|Enemy")
	float DestroyDelay = 3.0f;
};