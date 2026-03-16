#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "NTPlayer.generated.h"

struct FNetworkEntityState;
constexpr  float PositionTolerance = 5.0f;
constexpr  float RotationTolerance = 2.0f;
constexpr  float PitchTolerance = 2.0f;

UCLASS()
class TIMETHIEF_API ANTPlayer : public ACharacter
// Network Test Player
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ANTPlayer();
	virtual ~ANTPlayer();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
public:
	bool IsLocalPlayer() const;
	
public:
	void SetEntityId(uint32 InEntityId) { EntityId = InEntityId; }
	
public:
	void SetNetworkEntityState(const FNetworkEntityState& EntityState);
	
public:
	void SetNowPosition(const FVector& NewPosition) { NowPosition = NewPosition; }
	void SetDestPosition(const FVector& NewPosition) { DestPosition = NewPosition; InterpStartPosition = GetActorLocation(); InterpTargetPosition = NewPosition; InterpElapsed = 0.f; }
	
	uint32 GetEntityId() const { return EntityId; }
	FVector GetNowPosition() const { return NowPosition; }
	
	void SetTargetYaw(float InYaw) { TargetYaw = InYaw; }
	float GetNowYaw() const { return NowYaw; }
	
	void SetTargetPitch(float InPitch) { TargetPitch = InPitch; }
	float GetNowPitch() const { return NowPitch; }
	
	FVector GetMoveStepSpeed() const { return MoveStepSpeed; }
	
private:
	void SetYawApply(float InYaw);
	void SetPitchApply(float InPitch);
	
public:
	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
protected:
	uint32 EntityId = 0;
	
	FVector NowPosition = FVector::ZeroVector;
	float NowYaw = 0.0f;
	float NowPitch = 0.0f;
	
	FVector DestPosition = FVector::ZeroVector;
	float TargetYaw = 0.0f;
	float TargetPitch = 0.0f;
	
	FVector InterpStartPosition = FVector::ZeroVector;
	FVector InterpTargetPosition = FVector::ZeroVector;

	float InterpElapsed = 0.f;
	float InterpDuration = 0.1f;
	
	FVector MoveStepSpeed = FVector::ZeroVector;
	
};
