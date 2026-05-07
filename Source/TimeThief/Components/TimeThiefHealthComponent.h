#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interface/LifeObserver.h"
#include "TimeThiefHealthComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_FourParams(FOnHealthChangedSignature, const UTimeThiefHealthComponent*, float, float, AActor*);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeathSignature);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class TIMETHIEF_API UTimeThiefHealthComponent : public UActorComponent, public ILifeObserver
{
	GENERATED_BODY()

public:
	UTimeThiefHealthComponent();

	virtual void OnEndRespawn() override;
	
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
	
	UFUNCTION(BlueprintCallable, Category = "TimeThief|Health")
	void SetHealth(float MaxHP, float NewHP);
	
	UFUNCTION(BlueprintCallable, Category = "TimeThief|Health")
	void HandleHealthChanged(float NewHealth, float DeltaHealth);
	
	void Upgrade();
	
	FOnHealthChangedSignature OnHealthChanged_Delegate;

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
	
	UPROPERTY(EditDefaultsOnly, Category = "TimeThief|Health")
	float UpgradeAmount = 20.0f;
	
	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|Health")
	float CurrentHealth;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|Health")
	float MaxHealth;

	bool bIsDead = false;
	
	UPROPERTY()
	TObjectPtr<AActor> LastDamageInstigator;
};


