#pragma once

#include "CoreMinimal.h"
#include "Components/Combat/TimeThiefPawnCombatComponent.h"
#include "TimeThiefPlayerCombatComponent.generated.h"

class ATimeThiefWeaponBase;
class ATimeThiefRifle;
class USpringArmComponent;

UCLASS()
class TIMETHIEF_API UTimeThiefPlayerCombatComponent : public UTimeThiefPawnCombatComponent {
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Combat")
	ATimeThiefWeaponBase* SpawnAndRegisterWeapon(TSubclassOf<ATimeThiefWeaponBase> WeaponClass, bool bEquipImmediately = false);

	virtual void HandleInputPressed(FGameplayTag InputTag) override;
	virtual void HandleInputReleased(FGameplayTag InputTag) override;

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Combat|Aim")
	void StartAiming();

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Combat|Aim")
	void StopAiming();

	UFUNCTION(BlueprintPure, Category = "TimeThief|Combat|Aim")
	bool IsAiming() const { return bIsAiming; }

	UFUNCTION(BlueprintPure, Category = "TimeThief|Combat|Aim")
	float GetAimSpreadMultiplier() const { return AimSpreadMultiplier; }

protected:
	UPROPERTY(EditDefaultsOnly, Category = "TimeThief|Combat")
	TArray<TSubclassOf<ATimeThiefWeaponBase>> DefaultWeaponClasses;

	UPROPERTY(EditDefaultsOnly, Category = "TimeThief|Combat|Aim")
	float AimFOV = 60.0f;

	UPROPERTY(EditDefaultsOnly, Category = "TimeThief|Combat|Aim")
	float DefaultFOV = 90.0f;

	UPROPERTY(EditDefaultsOnly, Category = "TimeThief|Combat|Aim")
	float AimInterpSpeed = 15.0f;

	UPROPERTY(EditDefaultsOnly, Category = "TimeThief|Combat|Aim")
	float AimSpreadMultiplier = 0.3f;

	UPROPERTY(EditDefaultsOnly, Category = "TimeThief|Combat|Aim")
	float AimMovementSpeedMultiplier = 0.6f;

private:
	bool bIsAiming = false;
	float DefaultMaxWalkSpeed = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<USpringArmComponent> CachedCameraBoom;
};