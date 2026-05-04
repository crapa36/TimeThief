#pragma once

#include "CoreMinimal.h"
#include "Components/TimeThiefPawnExtensionComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "TimeThiefWireTypes.h"
#include "TimeThiefWireComponent.generated.h"

class UCharacterMovementComponent;
class ACharacter;
class UAnimInstance;
class UAnimMontage;
class UTimeThiefWirePhysics;
class UTimeThiefWireTargeting;
class UCableComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class APlayerCameraManager;
class UCameraShakeBase;
class USoundBase;
class UParticleSystem;

DECLARE_LOG_CATEGORY_EXTERN(LogWire, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWireStateChanged, EWireState, OldState, EWireState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWireAttached, const FVector&, AnchorPoint);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWireLaunched, const FVector&, StartPosition, const FVector&, Direction);

UCLASS(BlueprintType, meta=(BlueprintSpawnableComponent))
class TIMETHIEF_API UTimeThiefWireComponent : public UTimeThiefPawnExtensionComponent
{
	GENERATED_BODY()

public:
	UTimeThiefWireComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Wire")
	void FireWire();

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Wire")
	void ConfirmWireFire();

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Wire")
	void CancelWireFire();

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Wire")
	void ReleaseWire();

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Wire")
	void Jump();

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Wire|Network")
	void SimulateAttach(const FVector& RemoteAnchorPoint);

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Wire|Network")
	void SimulateDetach();

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Wire|Network")
	void SimulateLaunch(const FVector& RemoteStartPosition, const FVector& RemoteDirection);

	void HandleInputPressed(FGameplayTag InputTag);
	void SetMoveInput(const FVector2D& Input) { MoveInput = Input; }

	UFUNCTION(BlueprintPure, Category = "TimeThief|Wire")
	bool IsWireActive() const { return CurrentState != EWireState::Idle; }

	UFUNCTION(BlueprintPure, Category = "TimeThief|Wire")
	bool IsWireAttached() const { return CurrentState == EWireState::Attached; }

	UFUNCTION(BlueprintPure, Category = "TimeThief|Wire")
	bool CanFireWire() const;

	UFUNCTION(BlueprintPure, Category = "TimeThief|Wire")
	FVector GetAnchorPoint() const { return AnchorPoint; }

	UFUNCTION(BlueprintPure, Category = "TimeThief|Wire")
	float GetCurrentWireLength() const;

	UFUNCTION(BlueprintPure, Category = "TimeThief|Wire")
	EWireState GetWireState() const { return CurrentState; }

	UFUNCTION(BlueprintPure, Category = "TimeThief|Wire")
	float GetCooldownRemaining() const { return CooldownRemaining; }

	UFUNCTION(BlueprintPure, Category = "TimeThief|Wire")
	FVector GetWireStartLocation() const;

	UFUNCTION(BlueprintPure, Category = "TimeThief|Wire")
	FVector GetPullAnchorPoint() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void SetWireState(EWireState NewState);
	void UpdateCooldown(float DeltaTime);
	bool ShouldTickComponent() const;

	void LaunchWire();
	bool PlayWireFireMontage(bool& bOutHasFireNotify);
	void PlayAttachedWireMontage();
	bool DoesMontageContainWireFireNotify(const UAnimMontage* Montage) const;
	UAnimInstance* GetWireMontageAnimInstance() const;
	void PlayMontageOnWireMeshes(UAnimMontage* Montage, UAnimInstance* PrimaryAnimInstance);
	void BindWireFireAnimation(UAnimInstance* AnimInstance, UAnimMontage* Montage);
	void ClearWireFireAnimation(bool bClearMontageEndDelegate);
	FName GetWireFireNotifyEventName() const;
	void OnWireFireMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void OnWireFireMontageNotify(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload);

	UFUNCTION()
	void AnimNotify_WireFire();

	void UpdateFiringAnchor(float DeltaTime);
	void UpdateAttachedWire(float DeltaTime);
	void OnAnchorAttached();

	bool ShouldRelease(float DeltaTime);
	bool IsStuck(float DeltaTime);
	bool IsOnGroundTooLong(float DeltaTime);
	bool IsWireSnapping() const;
	bool IsFacingAwayFromWire() const;
	
	FVector GetAimDirection() const;
	float GetWireCableTautnessAlpha(float CurrentDistance) const;
	void ApplyWireCableStaticSettings(bool bRecreateSimulation);
	void UpdateWireVisuals();
	void UpdateTargetIndicator();

	void UpdateSpeedEffects(float DeltaTime);
	void ResetSpeedEffects(float DeltaTime);
	float GetSpeedEffectAlpha() const;

	void UpdateWireRotation(float DeltaTime);
	void ApplyWireRotationMode();
	void RestoreRotationMode();

public:
	UPROPERTY(BlueprintAssignable, Category = "TimeThief|Wire")
	FOnWireStateChanged OnWireStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "TimeThief|Wire")
	FOnWireAttached OnWireAttached;

	UPROPERTY(BlueprintAssignable, Category = "TimeThief|Wire")
	FOnWireLaunched OnWireLaunched;

protected:
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "Wire|Modules")
	TObjectPtr<UTimeThiefWirePhysics> WirePhysics;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "Wire|Modules")
	TObjectPtr<UTimeThiefWireTargeting> WireTargeting;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Settings")
	float MaxWireLength = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Settings")
	float WireFireSpeed = 4000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Settings")
	float WireCooldown = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Settings")
	float ArrivalDistance = 350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Settings")
	FName WireStartSocketName = FName("HandGrip_L");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Settings")
	float WireLengthUpdateTolerance = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Settings|Advanced", meta = (AdvancedDisplay))
	float StuckSpeedThreshold = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Settings|Advanced", meta = (AdvancedDisplay))
	float StuckCheckDelay = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Settings|Advanced", meta = (AdvancedDisplay))
	float AirControlOnWire = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Settings|Advanced", meta = (AdvancedDisplay))
	float WireReleaseLookDotThreshold = -0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Settings|Advanced", meta = (AdvancedDisplay, ClampMin = "0.0", UIMin = "0.0"))
	float PullAnchorHeightOffset = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Audio")
	TObjectPtr<USoundBase> FireSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Audio")
	TObjectPtr<USoundBase> AttachSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Visuals")
	TObjectPtr<UParticleSystem> AttachParticle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Visuals")
	float WireThickness = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Visuals")
	TObjectPtr<UMaterialInterface> WireMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Visuals|Cable|Shape", meta = (ClampMin = "1", ClampMax = "20", UIMin = "1", UIMax = "20"))
	int32 WireCableNumSegments = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Visuals|Cable|Shape", meta = (ClampMin = "1", ClampMax = "16", UIMin = "1", UIMax = "16"))
	int32 WireCableNumSides = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Visuals|Cable|Shape", meta = (UIMin = "0.1", UIMax = "8.0"))
	float WireCableTileMaterial = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Visuals|Cable|Simulation", meta = (ClampMin = "0.005", UIMin = "0.005", UIMax = "0.1"))
	float WireCableSubstepTime = 0.01f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Visuals|Cable|Simulation")
	bool bWireCableResetAfterTeleport = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Visuals|Cable|Simulation")
	bool bWireCableTeleportAfterReattach = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Visuals|Cable|Firing", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float FiringCableSlackMultiplier = 1.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Visuals|Cable|Firing", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float FiringCableGravityScale = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Visuals|Cable|Firing", meta = (ClampMin = "1", ClampMax = "16", UIMin = "1", UIMax = "16"))
	int32 FiringCableSolverIterations = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Visuals|Cable|Firing")
	bool bFiringCableEnableStiffness = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Visuals|Cable|Firing", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float FiringCableTeleportDistanceThreshold = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Visuals|Cable|Taut", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float TautCableSlackMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Visuals|Cable|Taut", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float TautCableGravityScale = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Visuals|Cable|Taut", meta = (ClampMin = "1", ClampMax = "16", UIMin = "1", UIMax = "16"))
	int32 TautCableSolverIterations = 16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Visuals|Cable|Taut")
	bool bTautCableEnableStiffness = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Visuals|Cable|Taut", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float TautCableTeleportDistanceThreshold = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Visuals")
	TObjectPtr<UStaticMesh> AnchorMeshTemplate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Visuals")
	FVector AnchorMeshScale = FVector(1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Visuals")
	FVector AnchorWireAttachOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Visuals")
	FRotator AnchorMeshRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Speed Effects")
	float SpeedEffectThreshold = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Speed Effects")
	float MaxFOVIncrease = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Speed Effects")
	float FOVInterpSpeed = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Speed Effects")
	float CameraShakeScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Speed Effects")
	TSubclassOf<UCameraShakeBase> WireSpeedShake;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Rotation")
	bool bOrientToVelocityOnWire = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Rotation")
	float WireRotationInterpSpeed = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Rotation")
	float WireRotationMinSpeed = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Rotation")
	float WireRotationForceAngleThreshold = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Animation")
	TObjectPtr<UAnimMontage> FireMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Animation")
	TObjectPtr<UAnimMontage> AttachedMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Animation")
	FName WireFireNotifyName = FName("WireFire");

private:
	UPROPERTY(Transient)
	TObjectPtr<ACharacter> CachedCharacter;

	UPROPERTY(Transient)
	TObjectPtr<UCharacterMovementComponent> CachedMovementComponent;

	UPROPERTY(Transient)
	TObjectPtr<UCableComponent> WireCable;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> AnchorMeshComponent;

	UPROPERTY(Transient)
	TObjectPtr<APlayerCameraManager> CachedCameraManager;

	UPROPERTY(Transient)
	TObjectPtr<UAnimInstance> PendingFireAnimInstance;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> PendingFireMontage;
	
	UPROPERTY()
	EWireState CurrentState = EWireState::Idle;

	UPROPERTY()
	FVector AnchorPoint = FVector::ZeroVector;

	UPROPERTY()
	FVector FireDirection = FVector::ZeroVector;

	UPROPERTY()
	FRotator AttachedAnchorRotation = FRotator::ZeroRotator;

	FVector2D MoveInput = FVector2D::ZeroVector;
	float CurrentFireDistance = 0.0f;
	float CooldownRemaining = 0.0f;
	float AttachedWireLength = 0.0f;
	float CachedAirControl = 0.0f;
	bool CachedOrientRotationToMovement = true;
	bool CachedUseControllerDesiredRotation = false;
	bool CachedUseControllerRotationYaw = false;
	float StuckCheckTimer = 0.0f;
	float GroundCheckTimer = 0.0f;
	float DefaultFOV = 90.0f;
	float CurrentFOVOffset = 0.0f;
	FName PendingWireFireNotifyName = NAME_None;
	FName PendingWireFireNotifyEventName = NAME_None;
	bool bPendingWireFire = false;
	bool bFireOnMontageEnded = false;
};
