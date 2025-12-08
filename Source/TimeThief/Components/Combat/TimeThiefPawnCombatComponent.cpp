#include "Components/Combat/TimeThiefPawnCombatComponent.h"
#include "Weapon/TimeThiefWeaponBase.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Logging/StructuredLog.h"

UTimeThiefPawnCombatComponent::UTimeThiefPawnCombatComponent() {
	PrimaryComponentTick.bCanEverTick = false;
	CurrentEquippedWeapon = nullptr;
}

void UTimeThiefPawnCombatComponent::RegisterSpawnedWeapon(FGameplayTag InWeaponTagToRegister, ATimeThiefWeaponBase* InWeaponToRegister, bool bRegisterAsEquippedWeapon) {
	if (CharacterCarriedWeaponMap.Contains(InWeaponTagToRegister)) {
		return;
	}

	CharacterCarriedWeaponMap.Emplace(InWeaponTagToRegister, InWeaponToRegister);

	if (bRegisterAsEquippedWeapon) {
		CurrentEquippedWeaponTag = InWeaponTagToRegister;
		CurrentEquippedWeapon = InWeaponToRegister;

		if (InWeaponToRegister) {
			if (ACharacter* OwningCharacter = GetOwningPawn<ACharacter>()) {
				InWeaponToRegister->AttachToComponent(OwningCharacter->GetMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale, InWeaponToRegister->GetSocketName());

				// 이 로직을 AnimInstance로 이전하여 중복을 제거합니다.
				/*
				if (TSubclassOf<UAnimInstance> AnimLayer = InWeaponToRegister->GetEquipAnimLayer()) {
					OwningCharacter->GetMesh()->LinkAnimClassLayers(AnimLayer);
				}
				*/
			}
		}
	}
}

ATimeThiefWeaponBase* UTimeThiefPawnCombatComponent::GetCharacterCarriedWeaponByTag(FGameplayTag InWeaponTagToGet) const {
	if (CharacterCarriedWeaponMap.Contains(InWeaponTagToGet)) {
		return CharacterCarriedWeaponMap.FindRef(InWeaponTagToGet);
	}
	return nullptr;
}

ATimeThiefWeaponBase* UTimeThiefPawnCombatComponent::GetCharacterCurrentEquippedWeapon() const {
	return CurrentEquippedWeapon;
}

void UTimeThiefPawnCombatComponent::ToggleWeaponCollision(bool bShouldEnable, EToggleDamageType ToggleDamageType) {
	if (ToggleDamageType == EToggleDamageType::CurrentEquippedWeapon) {
		if (IsValid(CurrentEquippedWeapon)) {
			USkeletalMeshComponent* WeaponMesh = CurrentEquippedWeapon->GetWeaponMesh();
			if (IsValid(WeaponMesh)) {
				if (bShouldEnable) {
					WeaponMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
				}
				else {
					WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				}
			}
		}
	}
}