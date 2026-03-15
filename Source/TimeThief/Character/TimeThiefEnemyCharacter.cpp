#include "Character/TimeThiefEnemyCharacter.h"
#include "Components/TimeThiefHealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ATimeThiefEnemyCharacter::ATimeThiefEnemyCharacter() {
}

void ATimeThiefEnemyCharacter::BeginPlay() {
	Super::BeginPlay();

	if (UTimeThiefHealthComponent* Health = GetHealthComponent()) {
		Health->OnDeath.AddDynamic(this, &ATimeThiefEnemyCharacter::OnDeath);
	}
}

void ATimeThiefEnemyCharacter::OnDeath(AActor* OwningActor) {
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->DisableMovement();
	GetMesh()->SetSimulatePhysics(true);
	GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));

	SetLifeSpan(DestroyDelay);
}