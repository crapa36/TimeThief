#pragma once

#include "CoreMinimal.h"
#include "Components/Combat/TimeThiefPawnCombatComponent.h"
#include "TimeThiefPlayerCombatComponent.generated.h"

class ATimeThiefMasterWeapon;
class UCameraComponent;
class UCharacterMovementComponent;

UCLASS()
class TIMETHIEF_API UTimeThiefPlayerCombatComponent : public UTimeThiefPawnCombatComponent {
	GENERATED_BODY()

public:
	UTimeThiefPlayerCombatComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual bool ShouldApplyRemoteFireYawRotation() const override { return false; }
	virtual void EquipWeapon(FGameplayTag WeaponTag) override;

	virtual void HandleInputPressed(FGameplayTag InputTag) override;
	virtual void HandleInputReleased(FGameplayTag InputTag) override;

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Combat|Aim")
	void StartAiming();

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Combat|Aim")
	void StopAiming();
	
	UFUNCTION(BlueprintPure, Category = "TimeThief|Combat|Aim")
	FVector GetWorldAimLocation() const { return CachedWorldAimLocation; }

	UFUNCTION(BlueprintPure, Category = "TimeThief|Combat")
	ATimeThiefMasterWeapon* GetMasterWeapon() const { return MasterWeaponPtr; }

	void SetMoveSpeedUpgradeBonus(float InMoveSpeedBonus);

	UFUNCTION(Server, Unreliable)
	void Server_SyncAim(float InAimYaw, float InAimPitch, float InCharacterYaw);
	
    int TurnDirection = 0;

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
	float TurnSpeed = 240.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "TimeThief|Combat|Aim")
	float AimTraceRange = 50000.0f;

	UPROPERTY(EditDefaultsOnly, Category = "TimeThief|Combat|Aim", meta = (ClampMin = "0.0", ClampMax = "179.9"))
	float AimYawOverflowTurnThreshold = 35.0f;
	
	bool bIsFireInputHeld = false;

private:
	void UpdateAimFOV(float DeltaTime);
	void UpdateLocalWorldAimLocation();
	void ApplyAimYawOverflowRotation(float DeltaTime);
	void SyncAimToServer();

	float DefaultMaxWalkSpeed = 0.0f;
	FVector CachedWorldAimLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	TWeakObjectPtr<UCameraComponent> CachedThirdPersonCamera;

	UPROPERTY(Transient)
	TWeakObjectPtr<UCameraComponent> CachedFirstPersonCamera;
};