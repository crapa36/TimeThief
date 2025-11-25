#include "GAS/TimeThiefAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"

UTimeThiefAttributeSet::UTimeThiefAttributeSet() {
	InitHealth(100.0f);
	InitMaxHealth(100.0f);
	InitTimeEnergy(100.0f);
	InitMaxTimeEnergy(100.0f);
}

void UTimeThiefAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const {
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UTimeThiefAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTimeThiefAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTimeThiefAttributeSet, TimeEnergy, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTimeThiefAttributeSet, MaxTimeEnergy, COND_None, REPNOTIFY_Always);
}

void UTimeThiefAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) {
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute()) {
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
	if (Attribute == GetTimeEnergyAttribute()) {
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxTimeEnergy());
	}
}

void UTimeThiefAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) {
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetHealthAttribute()) {
		// 체력 변경 시 추가 로직 (UI 갱신, 피격 모션 등)
	}
}

void UTimeThiefAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth) {
	GAMEPLAYATTRIBUTE_REPNOTIFY(UTimeThiefAttributeSet, Health, OldHealth);
}

void UTimeThiefAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth) {
	GAMEPLAYATTRIBUTE_REPNOTIFY(UTimeThiefAttributeSet, MaxHealth, OldMaxHealth);
}

void UTimeThiefAttributeSet::OnRep_TimeEnergy(const FGameplayAttributeData& OldTimeEnergy) {
	GAMEPLAYATTRIBUTE_REPNOTIFY(UTimeThiefAttributeSet, TimeEnergy, OldTimeEnergy);
}

void UTimeThiefAttributeSet::OnRep_MaxTimeEnergy(const FGameplayAttributeData& OldMaxTimeEnergy) {
	GAMEPLAYATTRIBUTE_REPNOTIFY(UTimeThiefAttributeSet, MaxTimeEnergy, OldMaxTimeEnergy);
}