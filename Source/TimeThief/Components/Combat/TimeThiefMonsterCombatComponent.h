#pragma once

#include "CoreMinimal.h"
#include "TimeThiefPawnCombatComponent.h"
#include "TimeThiefMonsterCombatComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class TIMETHIEF_API UTimeThiefMonsterCombatComponent : public UTimeThiefPawnCombatComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UTimeThiefMonsterCombatComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
	
public:
	virtual void Remote_AttackRequest(const FRemoteAttackNotify& AttackRequest) override;
	
};
