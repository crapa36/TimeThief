#include "Character/TimeThiefCharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/TimeThiefAbilitySystemComponent.h"
#include "GAS/TimeThiefAttributeSet.h"
#include "GAS/TimeThiefAbilitySet.h"
#include "Logging/StructuredLog.h"

ATimeThiefCharacterBase::ATimeThiefCharacterBase() {
	PrimaryActorTick.bCanEverTick = false;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	AbilitySystemComponent = CreateDefaultSubobject<UTimeThiefAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UTimeThiefAttributeSet>(TEXT("AttributeSet"));

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
}

UAbilitySystemComponent* ATimeThiefCharacterBase::GetAbilitySystemComponent() const {
	return AbilitySystemComponent;
}

UAttributeSet* ATimeThiefCharacterBase::GetAttributeSet() const {
	return AttributeSet;
}

void ATimeThiefCharacterBase::BeginPlay() {
	Super::BeginPlay();

	if (!StartupAbilitySet) {
		UE_LOGFMT(LogActor, Warning, "StartupAbilitySet is Missing in {Name}. No abilities will be granted.", GetName());
	}
}

void ATimeThiefCharacterBase::PossessedBy(AController* NewController) {
	Super::PossessedBy(NewController);

	InitAbilityActorInfo();
}

void ATimeThiefCharacterBase::OnRep_PlayerState() {
	Super::OnRep_PlayerState();

	InitAbilityActorInfo();
}

void ATimeThiefCharacterBase::InitAbilityActorInfo() {
	if (AbilitySystemComponent) {
		AbilitySystemComponent->InitAbilityActorInfo(this, this);

		if (HasAuthority() && StartupAbilitySet) {
			StartupAbilitySet->GiveToAbilitySystem(AbilitySystemComponent, this);
		}
	}
}