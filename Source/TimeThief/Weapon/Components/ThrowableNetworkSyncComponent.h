#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "ThrowableNetworkSyncComponent.generated.h"

class ANetworkActor;
class UProjectileMovementComponent;

UENUM(BlueprintType)
enum class EThrowableNetRole : uint8
{
	None,
	LocalOwner,     // 내가 던진 투척체: 로컬 물리 시뮬레이션 + 서버로 MoveSync 송신
	RemoteProxy,   // 남이 던진 투척체: 서버 MoveSync 수신 기반 지연 보간
};

USTRUCT(BlueprintType)
struct FThrowableMoveSnapshot
{
	GENERATED_BODY()

	UPROPERTY()
	uint32 ObjectId = 0;

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY()
	FVector Velocity = FVector::ZeroVector;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class TIMETHIEF_API UThrowableNetworkSyncComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UThrowableNetworkSyncComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
	
public:
	void InitializeAsLocalOwner(uint32 InObjectId);
	void InitializeAsRemoteProxy(uint32 InObjectId);
	
	uint32 GetObjectId() const { return ObjectId; }
	
	bool IsLocalOwner() const { return NetRole == EThrowableNetRole::LocalOwner; }
	bool IsRemoteProxy() const { return NetRole == EThrowableNetRole::RemoteProxy; }
	
	void PushRemoteSnapshot(const FThrowableMoveSnapshot& Snapshot);
	
	void SendExplosionSync();
	
private:
	void TickLocalOwner(float DeltaTime);
	void TickRemoteProxy(float DeltaTime);
	
	void SendMoveSyncToServer();
	void ApplyRemoteInterpolation(float DeltaTime);
	
	void DisableLocalProjectileSimulation();
	
private:
	UPROPERTY(EditAnywhere, Category="TimeThief|Throwable|Network")
	float MoveSendInterval = 0.1f; // 10Hz

	UPROPERTY(EditAnywhere, Category="TimeThief|Throwable|Network")
	float RemoteInterpolationDelay = 0.1f; // 100ms 지연 출력

	UPROPERTY(VisibleInstanceOnly, Category="TimeThief|Throwable|Network")
	EThrowableNetRole NetRole = EThrowableNetRole::None;

	UPROPERTY(VisibleInstanceOnly, Category="TimeThief|Throwable|Network")
	uint32 ObjectId = 0;

	float MoveSendElapsed = 0.0f;

	TArray<FThrowableMoveSnapshot> SnapshotBuffer;

	TWeakObjectPtr<ANetworkActor> CachedNetworkActor;
	TWeakObjectPtr<UProjectileMovementComponent> CachedProjectileMovement;

// Remote Proxy용 보간 상태
private:
	bool bHasInterpolationTarget = false;

	FVector InterpStartLocation = FVector::ZeroVector;
	FRotator InterpStartRotation = FRotator::ZeroRotator;

	FVector InterpTargetLocation = FVector::ZeroVector;
	FRotator InterpTargetRotation = FRotator::ZeroRotator;
	FVector InterpTargetVelocity = FVector::ZeroVector;

	float InterpElapsed = 0.0f;
	
};
