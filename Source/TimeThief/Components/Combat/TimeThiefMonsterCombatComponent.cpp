


#include "TimeThiefMonsterCombatComponent.h"

#include "Monster/TimeThiefMonster.h"


// Sets default values for this component's properties
UTimeThiefMonsterCombatComponent::UTimeThiefMonsterCombatComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UTimeThiefMonsterCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UTimeThiefMonsterCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                                     FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UTimeThiefMonsterCombatComponent::Remote_AttackRequest(const FRemoteAttackNotify& AttackRequest)
{
	// Super::Remote_AttackRequest(AttackRequest);
	
	ATimeThiefMonster* Monster = GetOwner<ATimeThiefMonster>();
	if (Monster == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("UTimeThiefMonsterCombatComponent::Remote_AttackRequest: Owner is not ATimeThiefMonster"));
		return;
	}

	Monster->HandleRemoteCombatRequest(AttackRequest);
}

