


#include "ThrowableNetworkSyncComponent.h"
#include "Actors/NetworkActor.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Network/NetworkGameInstanceSubsystem.h"


// Sets default values for this component's properties
UThrowableNetworkSyncComponent::UThrowableNetworkSyncComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

}


// Called when the game starts
void UThrowableNetworkSyncComponent::BeginPlay()
{
	Super::BeginPlay();

	CachedNetworkActor = Cast<ANetworkActor>(GetOwner());
	CachedProjectileMovement = GetOwner() ? GetOwner()->FindComponentByClass<UProjectileMovementComponent>() : nullptr;
}


// Called every frame
void UThrowableNetworkSyncComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                                   FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	switch (NetRole)
	{
	case EThrowableNetRole::LocalOwner:
		TickLocalOwner(DeltaTime);
		break;
		
	case EThrowableNetRole::RemoteProxy:
		TickRemoteProxy(DeltaTime);
		break;
		
	default:
		// UE_LOG(LogTemp, Error, TEXT("Unknown NetRole %d"), NetRole);
		break;
	}
}

void UThrowableNetworkSyncComponent::InitializeAsLocalOwner(uint32 InObjectId)
{
	ObjectId = InObjectId;
	NetRole = EThrowableNetRole::LocalOwner;

	CachedNetworkActor = Cast<ANetworkActor>(GetOwner());
	CachedProjectileMovement = GetOwner()
		? GetOwner()->FindComponentByClass<UProjectileMovementComponent>()
		: nullptr;
}

void UThrowableNetworkSyncComponent::InitializeAsRemoteProxy(uint32 InObjectId)
{
	ObjectId = InObjectId;
	NetRole = EThrowableNetRole::RemoteProxy;

	CachedNetworkActor = Cast<ANetworkActor>(GetOwner());
	CachedProjectileMovement = GetOwner()
		? GetOwner()->FindComponentByClass<UProjectileMovementComponent>()
		: nullptr;

	DisableLocalProjectileSimulation();
}

void UThrowableNetworkSyncComponent::PushRemoteSnapshot(const FThrowableMoveSnapshot& Snapshot)
{
	if (Snapshot.ObjectId != ObjectId)
	{
		UE_LOG(LogTemp, Warning, TEXT("Received snapshot for ObjectId %u, but this component is for ObjectId %u"),
			Snapshot.ObjectId,
			ObjectId);
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	SnapshotBuffer.Add(Snapshot);

	constexpr int32 MaxBufferSize = 8;
	if (SnapshotBuffer.Num() > MaxBufferSize)
	{
		SnapshotBuffer.RemoveAt(0, SnapshotBuffer.Num() - MaxBufferSize);
	}

	// 새 Snapshot이 들어온 순간 현재 위치를 Start로 잡음
	InterpStartLocation = Owner->GetActorLocation();
	InterpStartRotation = Owner->GetActorRotation();

	InterpTargetLocation = Snapshot.Location;
	InterpTargetRotation = Snapshot.Rotation;
	InterpTargetVelocity = Snapshot.Velocity;

	InterpElapsed = 0.0f;
	bHasInterpolationTarget = true;
}

void UThrowableNetworkSyncComponent::SendExplosionSync()
{
	AActor* Owner = GetOwner();
	if (!Owner || ObjectId == 0)
	{
		return;;
	}
	
	const FVector Location = Owner->GetActorLocation();
	
	if (auto* NGIS = UNetworkGameInstanceSubsystem::Get(this))
	{
		NGIS->SendGrenadeExplosion(ObjectId, Location);
	}
}

void UThrowableNetworkSyncComponent::TickLocalOwner(float DeltaTime)
{
	MoveSendElapsed += DeltaTime;
	
	if (MoveSendElapsed >= MoveSendInterval)
	{
		MoveSendElapsed = 0.0f;
		SendMoveSyncToServer();
	}
}

void UThrowableNetworkSyncComponent::TickRemoteProxy(float DeltaTime)
{
	ApplyRemoteInterpolation(DeltaTime);
}

void UThrowableNetworkSyncComponent::SendMoveSyncToServer()
{
	AActor* Owner = GetOwner();
	if (!Owner || ObjectId == 0)
	{
		return;;
	}
	
	const FVector Location = Owner->GetActorLocation();
	const FRotator Rotation = Owner->GetActorRotation();
	
	FVector Velocity = FVector::ZeroVector;
	if (UProjectileMovementComponent* ProjMove = CachedProjectileMovement.Get())
	{
		Velocity = ProjMove->Velocity;
	}
	
	FThrowableMoveSnapshot Info{ObjectId, Location, Rotation, Velocity};
	
	if (auto* NGIS = UNetworkGameInstanceSubsystem::Get(this))
	{
		NGIS->SendGrenadeMoveSync(Info);
	}
}

void UThrowableNetworkSyncComponent::ApplyRemoteInterpolation(float DeltaTime)
{
	AActor* Owner = GetOwner();
	if (!Owner || !bHasInterpolationTarget)
	{
		return;
	}

	constexpr float SnapDistance = 30.0f;

	const FVector CurrentLocation = Owner->GetActorLocation();
	const float Distance = FVector::Dist(CurrentLocation, InterpTargetLocation);

	// if (Distance > SnapDistance)
	// {
	// 	Owner->SetActorLocationAndRotation(InterpTargetLocation, InterpTargetRotation);
	// 	bHasInterpolationTarget = false;
	// 	return;
	// }

	const float Duration = FMath::Max(RemoteInterpolationDelay, KINDA_SMALL_NUMBER);

	InterpElapsed += DeltaTime;

	const float Alpha = FMath::Clamp(InterpElapsed / Duration, 0.0f, 1.0f);

	// 부드러운 감속/가속이 싫고 정확히 0.1초 내 도달시키려면 Alpha 그대로 사용
	const FVector NewLocation = FMath::Lerp(
		InterpStartLocation,
		InterpTargetLocation,
		Alpha);

	const FRotator NewRotation = FMath::Lerp(
		InterpStartRotation,
		InterpTargetRotation,
		Alpha);

	Owner->SetActorLocationAndRotation(NewLocation, NewRotation);

	if (Alpha >= 1.0f)
	{
		Owner->SetActorLocationAndRotation(InterpTargetLocation, InterpTargetRotation);
		bHasInterpolationTarget = false;
	}
}

void UThrowableNetworkSyncComponent::DisableLocalProjectileSimulation()
{
	if (UProjectileMovementComponent* ProjMove = CachedProjectileMovement.Get())
	{
		ProjMove->StopMovementImmediately();
		ProjMove->Deactivate();
	}
}

