#pragma once

#include "CoreMinimal.h"
#include "Components/TimeThiefPawnExtensionComponent.h"
#include "GameplayTagContainer.h"
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

	UPROPERTY(BlueprintAssignable, Category = "TimeThief|Wire")
	FOnWireStateChanged OnWireStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "TimeThief|Wire")
	FOnWireAttached OnWireAttached;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void SetWireState(EWireState NewState);
	void UpdateFiringAnchor(float DeltaTime);
	void UpdateAttachedWire(float DeltaTime);
	void UpdateCooldown(float DeltaTime);
	
	void OnAnchorAttached();
	void ApplyPendulumPhysics(UCharacterMovementComponent* Movement, const FVector& WireDirection, float WireLength, float DeltaTime);
	void ConstrainToWireLength(ACharacter* Character, UCharacterMovementComponent* Movement);
	
	bool ShouldReleaseByObstruction(UCharacterMovementComponent* Movement, const FVector& WireDirection, float CurrentDistance, float DeltaTime);
	
	FVector GetAimDirection() const;
	FVector GetWireStartLocation() const;
	FVector GetPlayerInputAcceleration(float DeltaTime) const;
	FVector GetTangentVelocity(const FVector& Velocity, const FVector& WireDirection) const;
	
	bool ShouldTickComponent() const;
	bool CheckAnchorCollision(const FVector& Start, const FVector& End, FHitResult& OutHit);
	
	void DrawWireLine() const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Settings")
	float MaxWireLength = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Settings")
	float WireFireSpeed = 5000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Settings")
	float WireCooldown = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Settings")
	float PullSpeed = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Settings")
	float ArrivalDistance = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Settings")
	float SwingInputAcceleration = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Settings")
	float WireStartZOffset = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Collision")
	TArray<TEnumAsByte<EObjectTypeQuery>> WireCollisionObjectTypes;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "Wire|Debug")
	FColor DebugWireColor = FColor::Cyan;

	UPROPERTY(EditAnywhere, Category = "Wire|Debug")
	float DebugWireThickness = 2.0f;
#endif

private:
	EWireState CurrentState = EWireState::Idle;
	FVector AnchorPoint = FVector::ZeroVector;
	FVector FireDirection = FVector::ZeroVector;
	FVector2D MoveInput = FVector2D::ZeroVector;
	float CurrentFireDistance = 0.0f;
	float CooldownRemaining = 0.0f;
	float AttachedWireLength = 0.0f;
	float CachedGravityScale = 1.0f;
	float CachedAirControl = 0.35f;
	float StuckCheckTimer = 0.0f;
	float InputAgainstWireTimer = 0.0f;

	static constexpr float StuckSpeedThreshold = 30.0f;
	static constexpr float StuckCheckDelay = 0.3f;
	static constexpr float WireGravityScale = 0.0f;
	static constexpr float WireTightThreshold = 0.95f;
	static constexpr float InputAgainstWireThreshold = 0.5f;
	static constexpr float InputAgainstWireDelay = 0.2f;
};

