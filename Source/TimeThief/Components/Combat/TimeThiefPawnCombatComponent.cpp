#include "Components/Combat/TimeThiefPawnCombatComponent.h"
#include "Weapon/TimeThiefWeaponBase.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"

UTimeThiefPawnCombatComponent::UTimeThiefPawnCombatComponent() {
	PrimaryComponentTick.bCanEverTick = false;
}

void UTimeThiefPawnCombatComponent::RegisterSpawnedWeapon(FGameplayTag InWeaponTagToRegister, ATimeThiefWeaponBase* InWeaponToRegister, bool bRegisterAsEquippedWeapon) {
	if (!InWeaponToRegister || !InWeaponTagToRegister.IsValid()) {
		return;
	}

	if (CharacterCarriedWeaponMap.Contains(InWeaponTagToRegister)) {
		return;
	}

	CharacterCarriedWeaponMap.Emplace(InWeaponTagToRegister, InWeaponToRegister);

	if (bRegisterAsEquippedWeapon) {
		EquipWeapon(InWeaponTagToRegister);
	}
}

void UTimeThiefPawnCombatComponent::EquipWeapon(FGameplayTag WeaponTag) {
	ATimeThiefWeaponBase* WeaponToEquip = GetCharacterCarriedWeaponByTag(WeaponTag);
	if (!WeaponToEquip) {
		return;
	}

	if (CurrentEquippedWeaponTag == WeaponTag) {
		return;
	}

	ACharacter* OwningCharacter = GetPawn<ACharacter>();
	if (!OwningCharacter) {
		return;
	}

	if (CurrentEquippedWeapon) {
		UnequipCurrentWeapon();
	}

	CurrentEquippedWeaponTag = WeaponTag;
	CurrentEquippedWeapon = WeaponToEquip;

	WeaponToEquip->SetActorHiddenInGame(false);
	AttachWeaponToSocket(WeaponToEquip);

	if (TSubclassOf<UAnimInstance> AnimLayer = WeaponToEquip->GetEquipAnimLayer()) {
		OwningCharacter->GetMesh()->LinkAnimClassLayers(AnimLayer);
	}

	PlayEquipMontage(WeaponToEquip);
}

void UTimeThiefPawnCombatComponent::UnequipCurrentWeapon() {
	if (!CurrentEquippedWeapon) {
		return;
	}

	ACharacter* OwningCharacter = GetPawn<ACharacter>();
	if (!OwningCharacter) {
		return;
	}

	CurrentEquippedWeapon->SetActorHiddenInGame(true);

	if (TSubclassOf<UAnimInstance> AnimLayer = CurrentEquippedWeapon->GetEquipAnimLayer()) {
		OwningCharacter->GetMesh()->UnlinkAnimClassLayers(AnimLayer);
	}

	CurrentEquippedWeaponTag = FGameplayTag();
	CurrentEquippedWeapon = nullptr;
}

void UTimeThiefPawnCombatComponent::AttachWeaponToSocket(ATimeThiefWeaponBase* Weapon) {
	if (!Weapon) {
		return;
	}

	ACharacter* OwningCharacter = GetPawn<ACharacter>();
	if (!OwningCharacter) {
		return;
	}

	FName SocketToUse = Weapon->GetSocketName();
	Weapon->AttachToComponent(OwningCharacter->GetMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale, SocketToUse);
}

void UTimeThiefPawnCombatComponent::PlayEquipMontage(ATimeThiefWeaponBase* Weapon) {
	if (!Weapon) {
		return;
	}

	UAnimMontage* EquipMontage = Weapon->GetEquipMontage();
	if (!EquipMontage) {
		return;
	}

	ACharacter* OwningCharacter = GetPawn<ACharacter>();
	if (!OwningCharacter) {
		return;
	}

	UAnimInstance* AnimInstance = OwningCharacter->GetMesh()->GetAnimInstance();
	if (AnimInstance) {
		AnimInstance->Montage_Play(EquipMontage);
	}
}

ATimeThiefWeaponBase* UTimeThiefPawnCombatComponent::GetCharacterCarriedWeaponByTag(FGameplayTag InWeaponTagToGet) const {
	if (const TObjectPtr<ATimeThiefWeaponBase>* FoundWeapon = CharacterCarriedWeaponMap.Find(InWeaponTagToGet)) {
		return *FoundWeapon;
	}
	return nullptr;
}

ATimeThiefWeaponBase* UTimeThiefPawnCombatComponent::GetCharacterCurrentEquippedWeapon() const {
	return CurrentEquippedWeapon;
}

void UTimeThiefPawnCombatComponent::ToggleWeaponCollision(bool bShouldEnable, EToggleDamageType ToggleDamageType) {
}

void UTimeThiefPawnCombatComponent::HandleInputPressed(FGameplayTag InputTag) {
}

void UTimeThiefPawnCombatComponent::HandleInputReleased(FGameplayTag InputTag) {
}
