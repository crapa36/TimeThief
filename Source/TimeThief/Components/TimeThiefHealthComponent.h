#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimeThiefHealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnHealthChangedSignature, UTimeThiefHealthComponent*, HealthComponent, float, OldHealth, float, NewHealth, AActor*, Instigator);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDeathSignature, AActor*, OwningActor);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class TIMETHIEF_API UTimeThiefHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTimeThiefHealthComponent();

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Health")
	void InitializeWithHealth(float InMaxHealth);

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Health")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Health")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Health")
	float GetHealthPercent() const;

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Health")
	bool IsDead() const { return bIsDead; }

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Health")
	void TakeDamage(float DamageAmount, AActor* DamageInstigator);

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Health")
	void Heal(float HealAmount, AActor* HealInstigator);

	UPROPERTY(BlueprintAssignable, Category = "TimeThief|Health")
	FOnHealthChangedSignature OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "TimeThief|Health")
	FOnDeathSignature OnDeath;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnTakeAnyDamageCallback(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);

	void HandleDeath();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Health")
	float DefaultMaxHealth = 100.0f;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|Health")
	float CurrentHealth;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|Health")
	float MaxHealth;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|Health")
	bool bIsDead = false;

	UPROPERTY()
	TObjectPtr<AActor> LastDamageInstigator;
};


