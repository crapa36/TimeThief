#include "Character/TimeThiefEnemyCharacter.h"
#include "GAS/TimeThiefAbilitySystemComponent.h"
#include "GAS/TimeThiefAttributeSet.h"

ATimeThiefEnemyCharacter::ATimeThiefEnemyCharacter() {
	
}

void ATimeThiefEnemyCharacter::BeginPlay() {
	Super::BeginPlay();
	InitAbilityActorInfo();
}

void ATimeThiefEnemyCharacter::InitAbilityActorInfo() {
	if (AbilitySystemComponent) {
		
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
}