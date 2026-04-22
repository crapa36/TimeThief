#pragma once

#include "CoreMinimal.h"
#include "Components/Combat/TimeThiefPawnCombatComponent.h"
#include "TimeThiefPlayerCombatComponent.generated.h"

class ATimeThiefMasterWeapon;
class UCameraComponent;
class UCharacterMovementComponent;
class UTimeThiefWireComponent;

UCLASS()
class TIMETHIEF_API UTimeThiefPlayerCombatComponent : public UTimeThiefPawnCombatComponent {
	GENERATED_BODY()

public:
	UTimeThiefPlayerCombatComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void Remote_SyncAimLocation(const FVector& Origin, const FVector& Direction) override;
	virtual bool ShouldApplyRemoteFireYawRotation() const override { return false; }
	virtual void EquipWeapon(FGameplayTag WeaponTag) override;

	virtual void HandleInputPressed(FGameplayTag InputTag) override;
	virtual void HandleInputReleased(FGameplayTag InputTag) override;

	void Local_StartAiming();

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Combat|Aim")
	void StartAiming();

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Combat|Aim")
	void StopAiming();

	UFUNCTION(BlueprintPure, Category = "TimeThief|Combat|Aim")
	float GetAimSpreadMultiplier() const { return AimSpreadMultiplier; }

	UFUNCTION(BlueprintPure, Category = "TimeThief|Combat")
	bool IsFiringWeapon() const;

	UFUNCTION(BlueprintPure, Category = "TimeThief|Combat|Rotation")
	bool ShouldUseWeaponControlRigRotation() const;

	UFUNCTION(BlueprintPure, Category = "TimeThief|Combat|Aim")
	FVector GetWorldAimLocation() const { return CachedWorldAimLocation; }

	UFUNCTION(BlueprintPure, Category = "TimeThief|Combat")
	ATimeThiefMasterWeapon* GetMasterWeapon() const { return MasterWeaponPtr; }

	void SetMoveSpeedUpgradeBonus(float InMoveSpeedBonus);

protected:
	virtual void OnEquipFinished() override;
	void ApplyUpgradeStatsToActiveWeapon();

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

	UPROPERTY(EditDefaultsOnly, Category = "TimeThief|Combat|Rotation")
	float MaxYawOffsetFromCamera = 45.0f;

	UPROPERTY(EditDefaultsOnly, Category = "TimeThief|Combat|Aim")
	float AimTraceRange = 50000.0f;

	bool bIsFireInputHeld = false;

private:
	void ApplyCombatRotationMode(bool bUseControllerFacing);
	bool ShouldUseControllerFacing() const;
	bool HasMovementIntent(const UCharacterMovementComponent* MovementComp) const;
	bool IsRotationManagedExternally() const;

	void UpdateCombatRotation();
	void UpdateAimFOV(float DeltaTime);

	void UpdateWorldAimLocation();
	void SnapRotationToAim();
	float GetClampedYawFromCamera(const ACharacter* OwningCharacter, float TargetYaw) const;
	bool TryGetFlatAimDirection(const ACharacter* OwningCharacter, FVector& OutFlatAimDirection) const;
	void ApplyThirdPersonAimRotation(ACharacter* OwningCharacter, float DeltaTime, bool bSnapRotation);

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