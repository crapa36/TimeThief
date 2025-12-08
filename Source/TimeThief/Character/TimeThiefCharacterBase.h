#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "TimeThiefCharacterBase.generated.h"

class UAbilitySystemComponent;
class UAttributeSet;
class UTimeThiefAbilitySystemComponent;
class UTimeThiefAttributeSet;
class UTimeThiefAbilitySet;
class UTimeThiefPawnCombatComponent;

UCLASS()
class TIMETHIEF_API ATimeThiefCharacterBase : public ACharacter, public IAbilitySystemInterface {
	GENERATED_BODY()

public:
	ATimeThiefCharacterBase();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UAttributeSet* GetAttributeSet() const;

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Combat")
	virtual UTimeThiefPawnCombatComponent* GetPawnCombatComponent() const { return nullptr; }

protected:
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	virtual void InitAbilityActorInfo();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Abilities")
	TObjectPtr<UTimeThiefAbilitySet> StartupAbilitySet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UTimeThiefAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UTimeThiefAttributeSet> AttributeSet;
};