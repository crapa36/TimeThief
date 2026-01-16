#pragma once

#include "CoreMinimal.h"
#include "Components/TimeThiefPawnExtensionComponent.h"
#include "GameplayTagContainer.h"
#include "UObject/ObjectPtr.h"
#include "TimeThiefWireComponent.generated.h"

class UCharacterMovementComponent;
class ACharacter;

DECLARE_LOG_CATEGORY_EXTERN(LogWire, Log, All);

UENUM(BlueprintType)
enum class EWireState : uint8
{
	Idle,
	Firing,
	Attached
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWireStateChanged, EWireState, OldState, EWireState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWireAttached, const FVector&, AnchorPoint);

UCLASS(BlueprintType, meta=(BlueprintSpawnableComponent))
class TIMETHIEF_API UTimeThiefWireComponent : public UTimeThiefPawnExtensionComponent
{
	GENERATED_BODY()

public:
	UTimeThiefWireComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Wire")
	void FireWire();

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Wire")
	void ReleaseWire();

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

	void HandleInputPressed(FGameplayTag InputTag);
	void SetMoveInput(const FVector2D& Input) { MoveInput = Input; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void SetWireState(EWireState NewState);
	void UpdateFiringAnchor(float DeltaTime);
	void UpdateAttachedWire(float DeltaTime);
	void UpdateCooldown(float DeltaTime);
	void OnAnchorAttached();

	FVector CalculatePullForce() const;
	FVector CalculateSwingInputForce() const;

	bool ShouldRelease(float DeltaTime);
	bool IsStuck(float DeltaTime);
	bool IsOnGroundTooLong(float DeltaTime);
	bool IsWireSnapping() const;
	bool IsFacingAwayFromWire() const;
	
	FVector GetAimDirection() const;
	FVector GetWireStartLocation() const;
	FVector GetTangentVelocity(const FVector& Velocity, const FVector& WireDirection) const;
	bool ShouldTickComponent() const;
	bool CheckAnchorCollision(const FVector& Start, const FVector& End, FHitResult& OutHit);
	bool FindBestAnchorTarget(FVector& OutTargetLocation);
	void DrawWireLine() const;

public:
	UPROPERTY(BlueprintAssignable, Category = "TimeThief|Wire")
	FOnWireStateChanged OnWireStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "TimeThief|Wire")
	FOnWireAttached OnWireAttached;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Settings")
	float MaxWireLength = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Settings")
	float WireFireSpeed = 4000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Settings")
	float WireCooldown = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Settings")
	float ArrivalDistance = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Settings")
	float SwingInputForce = 150000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Settings|AutoAim")
	float AutoAimRadius = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Settings|AutoAim")
	float AutoAimMaxAngle = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Settings|AutoAim")
	float MinTargetDistance = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Settings|AutoAim")
	bool bAllowFloorAttachment = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Settings")
	FName WireStartSocketName = FName("WireSocket");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Collision")
	TArray<TEnumAsByte<EObjectTypeQuery>> WireCollisionObjectTypes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Physics")
	float PullInForce = 150000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Physics")
	float CentrifugalMassMultiplier = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Physics")
	float MaxSwingSpeedMultiplier = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Physics")
	float MaxGroundTime = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Physics")
	float WireBreakSpeedThreshold = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Physics")
	float WireBreakAngleThreshold = -0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Physics")
	float WireStiffness = 5000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Physics")
	float WireDamping = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Physics")
	float GravityMultiplierOnWire = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Physics")
	float WireLengthUpdateTolerance = 5.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Physics")
	float SwingDragCoefficient = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Settings|Advanced", meta = (AdvancedDisplay))
	float StuckSpeedThreshold = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Settings|Advanced", meta = (AdvancedDisplay))
	float StuckCheckDelay = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Settings|Advanced", meta = (AdvancedDisplay))
	float MinWireLengthForPhysics = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Settings|Advanced", meta = (AdvancedDisplay))
	float AirControlOnWire = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Settings|Advanced", meta = (AdvancedDisplay))
	float WireReleaseLookDotThreshold = -0.2f;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "Wire|Debug")
	FColor DebugWireColor = FColor::Cyan;

	UPROPERTY(EditAnywhere, Category = "Wire|Debug")
	float DebugWireThickness = 2.0f;
#endif

private:
	UPROPERTY()
	TObjectPtr<ACharacter> CachedCharacter;

	UPROPERTY()
	TObjectPtr<UCharacterMovementComponent> CachedMovementComponent;
	
	UPROPERTY()
	EWireState CurrentState = EWireState::Idle;

	UPROPERTY()
	FVector AnchorPoint = FVector::ZeroVector;

	UPROPERTY()
	FVector FireDirection = FVector::ZeroVector;

	FVector2D MoveInput = FVector2D::ZeroVector;
	float CurrentFireDistance = 0.0f;
	float CooldownRemaining = 0.0f;
	float AttachedWireLength = 0.0f;
	float CachedGravityScale = 1.0f;
	float CachedAirControl = 0.0f;
	float StuckCheckTimer = 0.0f;
	float GroundCheckTimer = 0.0f;

	FVector CurrentWireDirection = FVector::ZeroVector;
	float CurrentWireDistance = 0.0f;
};
