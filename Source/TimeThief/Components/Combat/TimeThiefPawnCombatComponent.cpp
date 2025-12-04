#include "Components/Combat/TimeThiefPawnCombatComponent.h"
#include "Weapon/TimeThiefWeaponBase.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"

void UTimeThiefPawnCombatComponent::RegisterSpawnedWeapon(FGameplayTag InWeaponTagToRegister, ATimeThiefWeaponBase* InWeaponToRegister, bool bRegisterAsEquippedWeapon) {
	if (CharacterCarriedWeaponMap.Contains(InWeaponTagToRegister)) {
		return;
	}

	CharacterCarriedWeaponMap.Emplace(InWeaponTagToRegister, InWeaponToRegister);

	if (bRegisterAsEquippedWeapon) {
		CurrentEquippedWeaponTag = InWeaponTagToRegister;

		// [수정된 부분] 무기가 유효하다면 캐릭터 손에 붙이고 애니메이션 레이어를 적용합니다.
		if (InWeaponToRegister) {
			if (ACharacter* OwningCharacter = GetOwningPawn<ACharacter>()) {
				// 1. 무기를 캐릭터 메시의 소켓에 부착
				InWeaponToRegister->AttachToComponent(OwningCharacter->GetMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale, InWeaponToRegister->GetSocketName());

				// 2. 무기에 설정된 애니메이션 레이어(AnimBP)를 캐릭터에 적용
				if (TSubclassOf<UAnimInstance> AnimLayer = InWeaponToRegister->GetEquipAnimLayer()) {
					OwningCharacter->GetMesh()->LinkAnimClassLayers(AnimLayer);
				}
			}
		}
	}
}

ATimeThiefWeaponBase* UTimeThiefPawnCombatComponent::GetCharacterCarriedWeaponByTag(FGameplayTag InWeaponTagToGet) const {
	if (CharacterCarriedWeaponMap.Contains(InWeaponTagToGet)) {
		return *CharacterCarriedWeaponMap.Find(InWeaponTagToGet);
	}
	return nullptr;
}

ATimeThiefWeaponBase* UTimeThiefPawnCombatComponent::GetCharacterCurrentEquippedWeapon() const {
	if (!CurrentEquippedWeaponTag.IsValid()) {
		return nullptr;
	}
	return GetCharacterCarriedWeaponByTag(CurrentEquippedWeaponTag);
}