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

				// --- [디버깅] 애니메이션 레이어 링크 시도 ---
				TSubclassOf<UAnimInstance> AnimLayer = InWeaponToRegister->GetEquipAnimLayer();

				if (AnimLayer) {
					UE_LOG(LogTemp, Warning, TEXT("[CombatComp] Found AnimLayer in Weapon: %s. Trying to Link..."), *AnimLayer->GetName());

					OwningCharacter->GetMesh()->LinkAnimClassLayers(AnimLayer);

					UE_LOG(LogTemp, Warning, TEXT("[CombatComp] LinkAnimClassLayers called successfully."));
				}
				else {
					UE_LOG(LogTemp, Error, TEXT("[CombatComp] !!! AnimLayer is NULL in Weapon BP !!! Check 'Equip Anim Layer' in BP_Rifle."));
				}
				// ------------------------------------------
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