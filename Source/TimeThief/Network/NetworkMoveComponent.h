#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NetworkMoveComponent.generated.h"


enum class ENetworkActionPhase : uint8;
struct FNetworkActionEvent;
class IMovableNetworkEntityInterface;
struct FMoveSyncData;
struct FNetworkEntityState;
class UNetworkEntityComponent;
class UNetworkGameInstanceSubsystem;
DECLARE_MULTICAST_DELEGATE_OneParam(FOnRemoteAcitonNotify, const FNetworkActionEvent&);

UCLASS(ClassGroup=(Network), meta=(BlueprintSpawnableComponent))
class TIMETHIEF_API UNetworkMoveComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UNetworkMoveComponent();
	
public:
	FOnRemoteAcitonNotify OnRemoteActionNotify;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
	
public:
	void ApplyNetworkState(const FNetworkEntityState& EntityState);
	
	void SetMovementUpdateInterval(float InInterval);
	
	FVector GetMoveStep() const { return MoveStep; }
	
public:
	void HandleActionEvent(const FNetworkActionEvent& ActionEvent);
	
protected:
	void ApplyActionEvent(const FNetworkActionEvent& ActionEvent);
	void ApplyJumpAction(ENetworkActionPhase Phase);
	void ApplyCrouchAction(ENetworkActionPhase Phase);	
	
public:
	bool BuildMoveSyncData(FMoveSyncData& OutSyncData) const;
	
public:
	bool IsCloseEnoughPosition(const FVector& CurrentPosition) const;
	bool IsCloseEnoughYaw(float CurrentYaw) const;
	bool IsCloseEnoughPitch(float CurrentPitch) const;
	
private:
	void TickLocal(float DeltaTime);
	void TickRemote(float DeltaTime);
	void TickServer(float DeltaTime);
	
	void ApplyRemoteInterpolation(float DeltaTime);
	void SnapToTarget();
	
private:
	bool CanSendMovePacket() const;
	bool CanApplyRemoteInterpolation() const;
	IMovableNetworkEntityInterface* GetMovableOwner() const;
	
private:
	UNetworkGameInstanceSubsystem* GetNetworkGameInstanceSubsystem();
	
private:
	UPROPERTY()
	TObjectPtr<UNetworkGameInstanceSubsystem> NGIS = nullptr;
	
private:
	UPROPERTY()
	TObjectPtr<UNetworkEntityComponent> NetworkEntityComponent = nullptr;
	
private:
	float StartYaw = 0.0f;
	float TargetYaw = 0.0f;
	
	float StartPitch = 0.0f;
	float TargetPitch = 0.0f;
	
	FVector2D StartVelocity = FVector2D::ZeroVector;
	FVector2D TargetVelocity = FVector2D::ZeroVector;
	
	EMovementMode RecentMovementMode = EMovementMode::MOVE_None;
	
	FVector InterpStartPosition = FVector::ZeroVector;
	FVector InterpTargetPosition = FVector::ZeroVector;
	FVector MoveStep = FVector::ZeroVector;
	
	float InterpElapsed = 0.0f;
	float InterpDuration = 0.1f;	// TODO: 패킷 간격과 네트워크 지연을 고려해서 적절한 보간 지속 시간 설정 필요 (임시로 0.1초로 설정)
	
	float SendMoveElapsed = 0.0f;
	float SendMoveInterval = 0.1f;	// TODO: 패킷 간격과 네트워크 지연을 고려해서 적절한 이동 패킷 전송 간격 설정 필요 (임시로 0.1초로 설정)
	
	// TEMP
	float PositionTolerance = 5.0f;
	float RotationTolerance = 2.0f;
	float PitchTolerance = 2.0f;
	
};
