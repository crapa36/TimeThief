#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"

#include "Network/State/NetworkControlType.h"
#include "Network/NetworkEntityInterface.h"

#include "NTPlayer.generated.h"

class UNetworkEntityComponent;
struct FNetworkEntityState;

constexpr  float PositionTolerance = 5.0f;
constexpr  float RotationTolerance = 2.0f;
constexpr  float PitchTolerance = 2.0f;

UCLASS()
class TIMETHIEF_API ANTPlayer : public ACharacter, public INetworkEntityInterface
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
	virtual class UNetworkEntityComponent* GetNetworkEntityComponent() const override;
	
public:
	bool IsLocalPlayer() const;
	
public:
	void InitializeNetworkEntity(uint32 InEntityId, ENetworkControlType InControlType);
	void SetNetworkEntityState(const FNetworkEntityState& EntityState);
	
public:
	void SetNowPosition(const FVector& NewPosition) { NowPosition = NewPosition; }
	void SetDestPosition(const FVector& NewPosition) { DestPosition = NewPosition; InterpStartPosition = GetActorLocation(); InterpTargetPosition = NewPosition; InterpElapsed = 0.f; }
	
	uint32 GetEntityId() const;
	FVector GetNowPosition() const { return NowPosition; }
	
	void SetTargetYaw(float InYaw) { TargetYaw = InYaw; }
	float GetNowYaw() const { return NowYaw; }
	
	void SetTargetPitch(float InPitch) { TargetPitch = InPitch; }
	float GetNowPitch() const { return NowPitch; }
	
private:
	void SetYawApply(float InYaw);
	void SetPitchApply(float InPitch);
	
public:
	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	
private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Network", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UNetworkEntityComponent> NetworkEntityComponent = nullptr;
	
protected:
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
	
};
