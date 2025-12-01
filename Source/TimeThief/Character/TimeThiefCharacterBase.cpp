#include "Character/TimeThiefCharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/TimeThiefAbilitySystemComponent.h"
#include "GAS/TimeThiefAttributeSet.h"
#include "Weapon/TimeThiefWeaponBase.h"
#include "Net/UnrealNetwork.h"
#include "GAS/TimeThiefAbilitySet.h"

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
}

void ATimeThiefCharacterBase::InitAbilityActorInfo() {
	if (HasAuthority() && StartupAbilitySet) {
		StartupAbilitySet->GiveToAbilitySystem(AbilitySystemComponent, this);
	}
}

void ATimeThiefCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const {
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATimeThiefCharacterBase, CurrentWeapon);
}

void ATimeThiefCharacterBase::SetCurrentWeapon(ATimeThiefWeaponBase* NewWeapon) {
	if (CurrentWeapon) {
		CurrentWeapon->Destroy();
	}

	CurrentWeapon = NewWeapon;

	if (CurrentWeapon) {
		CurrentWeapon->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale, CurrentWeapon->GetSocketName());
		if (CurrentWeapon->GetEquipAnimLayer()) {
			GetMesh()->LinkAnimClassLayers(CurrentWeapon->GetEquipAnimLayer());
		}
	}
}