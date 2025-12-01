#include "GAS/Abilities/TimeThiefGA_EquipWeapon.h"
#include "Character/TimeThiefCharacterBase.h"
#include "Weapon/TimeThiefWeaponBase.h"
#include "AbilitySystemComponent.h"
#include "Components/Combat/TimeThiefPawnCombatComponent.h"

UTimeThiefGA_EquipWeapon::UTimeThiefGA_EquipWeapon() {
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UTimeThiefGA_EquipWeapon::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) {
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ATimeThiefCharacterBase* Character = GetTimeThiefCharacterFromActorInfo();
	if (!Character || !WeaponClass) {
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Character;
	SpawnParams.Instigator = Character;

	ATimeThiefWeaponBase* NewWeapon = GetWorld()->SpawnActor<ATimeThiefWeaponBase>(WeaponClass, Character->GetActorTransform(), SpawnParams);

	if (NewWeapon) {
		if (UTimeThiefPawnCombatComponent* CombatComp = Character->GetPawnCombatComponent()) {
			CombatComp->RegisterSpawnedWeapon(NewWeapon->GetWeaponTag(), NewWeapon, true);
		}

		UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
		if (ASC) {
			for (const TSubclassOf<UGameplayAbility>& Ability : NewWeapon->GetDefaultAbilities()) {
				FGameplayAbilitySpec Spec(Ability, 1, -1, Character);
				ASC->GiveAbility(Spec);
			}
		}
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UTimeThiefGA_EquipWeapon::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) {
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}