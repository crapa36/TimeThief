#include "GAS/Abilities/TimeThiefGA_EquipWeapon.h"
#include "Character/TimeThiefCharacterBase.h"
#include "Weapon/TimeThiefWeaponBase.h"
#include "AbilitySystemComponent.h"
#include "Components/Combat/TimeThiefPawnCombatComponent.h"
#include "Logging/StructuredLog.h"

UTimeThiefGA_EquipWeapon::UTimeThiefGA_EquipWeapon() {
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UTimeThiefGA_EquipWeapon::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) {
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ATimeThiefCharacterBase* Character = GetTimeThiefCharacterFromActorInfo();

	if (!Character || !WeaponClass) {
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}

	UTimeThiefPawnCombatComponent* CombatComp = Character->GetPawnCombatComponent();
	if (!CombatComp) {
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}

	const ATimeThiefWeaponBase* WeaponCDO = Cast<ATimeThiefWeaponBase>(WeaponClass->GetDefaultObject());
	if (WeaponCDO) {
		FGameplayTag WeaponTag = WeaponCDO->GetWeaponTag();

		if (CombatComp->GetCharacterCarriedWeaponByTag(WeaponTag)) {
			EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
			return;
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Character;
	SpawnParams.Instigator = Character;

	ATimeThiefWeaponBase* NewWeapon = GetWorld()->SpawnActor<ATimeThiefWeaponBase>(WeaponClass, Character->GetActorTransform(), SpawnParams);

	if (NewWeapon) {
		CombatComp->RegisterSpawnedWeapon(NewWeapon->GetWeaponTag(), NewWeapon, true);

		UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
		if (ASC) {
			for (const TSubclassOf<UGameplayAbility>& Ability : NewWeapon->GetDefaultAbilities()) {
				if (Ability) {
					FGameplayAbilitySpec Spec(Ability, 1, INDEX_NONE, Character);
					ASC->GiveAbility(Spec);
				}
			}
		}
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
}

void UTimeThiefGA_EquipWeapon::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) {
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, false, bWasCancelled);
}