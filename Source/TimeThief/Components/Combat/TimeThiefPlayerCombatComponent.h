#pragma once

#include "CoreMinimal.h"
#include "Components/Combat/TimeThiefPawnCombatComponent.h"
#include "TimeThiefPlayerCombatComponent.generated.h"

class ATimeThiefWeaponBase;
class UCameraComponent;
class UCharacterMovementComponent;
class UTimeThiefWireComponent;

UCLASS()
class TIMETHIEF_API UTimeThiefPlayerCombatComponent : public UTimeThiefPawnCombatComponent {
	GENERATED_BODY()

public:
	UTimeThiefPlayerCombatComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Combat")
	ATimeThiefWeaponBase* SpawnAndRegisterWeapon(TSubclassOf<ATimeThiefWeaponBase> WeaponClass, bool bEquipImmediately = false, FGameplayTag PreferredWeaponTag = FGameplayTag());

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

	UFUNCTION(BlueprintPure, Category = "TimeThief|Combat")
	bool IsFiringWeapon() const;

	UFUNCTION(BlueprintPure, Category = "TimeThief|Combat|Rotation")
	bool ShouldUseWeaponControlRigRotation() const;

	UFUNCTION(BlueprintPure, Category = "TimeThief|Combat|Aim")
	FVector GetWorldAimLocation() const { return CachedWorldAimLocation; }

protected:
	virtual void OnEquipFinished() override;

	UPROPERTY(EditDefaultsOnly, Category = "TimeThief|Combat")
	TArray<TSubclassOf<ATimeThiefWeaponBase>> DefaultWeaponClasses;

	UPROPERTY(EditDefaultsOnly, Category = "TimeThief|Combat")
	TMap<FGameplayTag, FGameplayTag> InputToWeaponTagMap;

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

	UPROPERTY(EditDefaultsOnly, Category = "TimeThief|Combat|Rotation")
	float CombatRotationRate = 720.0f;

	UPROPERTY(EditDefaultsOnly, Category = "TimeThief|Combat|Rotation")
	float PostFireRotationDelay = 0.5f;

	bool bIsFireInputHeld = false;

private:
	void EquipOrSpawnWeaponByTag(FGameplayTag WeaponTag);
	TSubclassOf<ATimeThiefWeaponBase> FindDefaultWeaponClassByTag(FGameplayTag WeaponTag) const;
	FGameplayTag InferWeaponTagFromClass(TSubclassOf<ATimeThiefWeaponBase> WeaponClass) const;

	void ApplyCombatRotationMode(bool bUseControllerFacing);
	bool ShouldUseControllerFacing() const;
	bool HasMovementIntent(const UCharacterMovementComponent* MovementComp) const;
	bool IsRotationManagedExternally() const;

	void UpdateCombatRotation();
	void UpdateAimFOV(float DeltaTime);

	void UpdateWorldAimLocation();
	void SnapRotationToAim();

	bool bIsAiming = false;
	float DefaultMaxWalkSpeed = 0.0f;
	FRotator DefaultRotationRate = FRotator(0.0f, 500.0f, 0.0f);
	bool bDefaultOrientRotationToMovement = true;
	bool bDefaultUseControllerDesiredRotation = false;
	bool bDefaultUseControllerRotationYaw = false;
	FVector CachedWorldAimLocation = FVector::ZeroVector;
	float LastFireTime = 0.0f;

	UPROPERTY(Transient)
	TWeakObjectPtr<UCameraComponent> CachedThirdPersonCamera;

	UPROPERTY(Transient)
	TWeakObjectPtr<UCameraComponent> CachedFirstPersonCamera;

	UPROPERTY(Transient)
	mutable TWeakObjectPtr<UTimeThiefWireComponent> CachedWireComponent;
};