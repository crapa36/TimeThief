#include "GAS/Abilities/TimeThiefGA_EquipWeapon.h"
#include "Character/TimeThiefCharacterBase.h"
#include "Weapon/TimeThiefWeaponBase.h"
#include "AbilitySystemComponent.h"

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

	// 1. 무기 스폰
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Character;
	SpawnParams.Instigator = Character;

	ATimeThiefWeaponBase* NewWeapon = GetWorld()->SpawnActor<ATimeThiefWeaponBase>(WeaponClass, Character->GetActorTransform(), SpawnParams);

	if (NewWeapon) {
		// 2. 캐릭터에게 무기 장착 요청 (소켓 부착 및 애니메이션 레이어 링크)
		Character->SetCurrentWeapon(NewWeapon);

		// 3. 무기가 가진 어빌리티 부여 (Grant Abilities)
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