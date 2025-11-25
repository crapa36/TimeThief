#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "TimeThiefAttributeSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class TIMETHIEF_API UTimeThiefAttributeSet : public UAttributeSet {
	GENERATED_BODY()

public:
	UTimeThiefAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

public:
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Vital Attributes")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UTimeThiefAttributeSet, Health);

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Vital Attributes")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UTimeThiefAttributeSet, MaxHealth);

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_TimeEnergy, Category = "Vital Attributes")
	FGameplayAttributeData TimeEnergy;
	ATTRIBUTE_ACCESSORS(UTimeThiefAttributeSet, TimeEnergy);

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxTimeEnergy, Category = "Vital Attributes")
	FGameplayAttributeData MaxTimeEnergy;
	ATTRIBUTE_ACCESSORS(UTimeThiefAttributeSet, MaxTimeEnergy);

protected:
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldHealth);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);

	UFUNCTION()
	void OnRep_TimeEnergy(const FGameplayAttributeData& OldTimeEnergy);

	UFUNCTION()
	void OnRep_MaxTimeEnergy(const FGameplayAttributeData& OldMaxTimeEnergy);
};