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
class ATimeThiefWeaponBase;

UCLASS()
class TIMETHIEF_API ATimeThiefCharacterBase : public ACharacter, public IAbilitySystemInterface {
	GENERATED_BODY()

public:
	ATimeThiefCharacterBase();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	
	UAttributeSet* GetAttributeSet() const;

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void SetCurrentWeapon(ATimeThiefWeaponBase* NewWeapon);

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	ATimeThiefWeaponBase* GetCurrentWeapon() const { return CurrentWeapon; }

protected:
	virtual void BeginPlay() override;
	virtual void InitAbilityActorInfo();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UTimeThiefAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UTimeThiefAttributeSet> AttributeSet;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<ATimeThiefWeaponBase> CurrentWeapon;
};