


#include "NetworkMoveComponent.h"

#include "NetworkGameInstanceSubsystem.h"
#include "Network/NetworkEntityComponent.h"
#include "Network/MovableNetworkEntityInterface.h"
#include "Network/State/NetworkControlType.h"
#include "Network/State/NetworkEntityState.h"
#include "Network/State/MoveSyncData.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"


class FTimeThiefMovementAccessor : public UCharacterMovementComponent
{
public:
	static void SetAcceleration(UCharacterMovementComponent* TargetCMC, const FVector& NewAccel)
	{
		if (TargetCMC)
		{
			static_cast<FTimeThiefMovementAccessor*>(TargetCMC)->Acceleration = NewAccel;
		}
	}
};

// Sets default values for this component's properties
UNetworkMoveComponent::UNetworkMoveComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UNetworkMoveComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	AActor* Owner = GetOwner();
	if (Owner)
	{
		NetworkEntityComponent = Owner->FindComponentByClass<UNetworkEntityComponent>();
	}
	
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			NGIS = GameInstance->GetSubsystem<UNetworkGameInstanceSubsystem>();
		}
	}
	
}


// Called every frame
void UNetworkMoveComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                          FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
	AActor* Owner = GetOwner();
	if (Owner == nullptr || NetworkEntityComponent == nullptr)
	{
		return;
	}
	
	IMovableNetworkEntityInterface* Movable = GetMovableOwner();
	if (Movable == nullptr)
	{
		return;
	}

	switch (NetworkEntityComponent->GetControlType())
	{
	case ENetworkControlType::Local:
		TickLocal(DeltaTime);
		break;
		
	case ENetworkControlType::Remote:
		TickRemote(DeltaTime);
		break;
		
	case ENetworkControlType::ServerAuth:
		TickServer(DeltaTime);
		break;
		
	default:
		break;
		
	}
}

void UNetworkMoveComponent::ApplyNetworkState(const FNetworkEntityState& EntityState)
{
	if (NetworkEntityComponent == nullptr)
	{
		return;
	}
	
	if (!NetworkEntityComponent->ShouldApplyNetworkState())
	{
		return;
	}
	
	AActor* Owner = GetOwner();
	IMovableNetworkEntityInterface* Movable = Cast<IMovableNetworkEntityInterface>(Owner);
	if (Owner == nullptr || Movable == nullptr)
	{
		return;
	}
	
	const FVector CurrentPosition = Movable->GetNetworkLocation();
	const float CurrentYaw = Movable->GetNetworkYaw();
	const float CurrentPitch = Movable->GetNetworkPitch();
	const float CurrentSpeed = Movable->GetNetworkSpeed();
	
	InterpStartPosition = CurrentPosition;
	InterpTargetPosition = EntityState.Position;
	
	StartYaw = CurrentYaw;
	TargetYaw = EntityState.Yaw;
	
	StartPitch = CurrentPitch;
	TargetPitch = EntityState.Pitch;
	
	StartSpeed = CurrentSpeed;
	TargetSpeed = EntityState.Speed;

	InterpElapsed = 0.0f;
}

void UNetworkMoveComponent::SetMovementUpdateInterval(float InInterval)
{
	if (InInterval <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetMovementUpdateInterval failed: Interval must be greater than zero."));
		return;
	}
	
	InterpDuration = InInterval;
	SendMoveInterval = InInterval;
}

bool UNetworkMoveComponent::BuildMoveSyncData(FMoveSyncData& OutSyncData) const
{
	const AActor* Owner = GetOwner();
	const IMovableNetworkEntityInterface* Movable = Cast<IMovableNetworkEntityInterface>(Owner);
	if (Owner == nullptr || Movable == nullptr)
	{
		return false;
	}
	
	OutSyncData.Position = Movable->GetNetworkLocation();
	OutSyncData.Yaw = Movable->GetNetworkYaw();
	OutSyncData.Pitch = Movable->GetLocalControlPitch();
	OutSyncData.Speed = Movable->GetNetworkSpeed();
	
	return true;
}

bool UNetworkMoveComponent::IsCloseEnoughPosition(const FVector& CurrentPosition) const
{
	return FVector::DistSquared(CurrentPosition, InterpTargetPosition) <= FMath::Square(PositionTolerance);
}

bool UNetworkMoveComponent::IsCloseEnoughYaw(float CurrentYaw) const
{
	return FMath::Abs(FMath::FindDeltaAngleDegrees(CurrentYaw, TargetYaw)) <= RotationTolerance;
}

bool UNetworkMoveComponent::IsCloseEnoughPitch(float CurrentPitch) const
{
	return FMath::Abs(FMath::FindDeltaAngleDegrees(CurrentPitch, TargetPitch)) <= PitchTolerance;
}

void UNetworkMoveComponent::TickLocal(float DeltaTime)
{
	AActor* Owner = GetOwner();
	IMovableNetworkEntityInterface* Movable = GetMovableOwner();
	if (Owner == nullptr || Movable == nullptr)
	{
		return;
	}

	MoveStep = Owner->GetVelocity();
	float CurrentPitch = Movable->GetLocalControlPitch();
	Movable->SetNetworkPitch(CurrentPitch);
	float CurrentSpeed = Movable->GetLocalControlSpeed();
	Movable->SetNetworkSpeed(CurrentSpeed);
	
	SendMoveElapsed += DeltaTime;
	
	if (!CanSendMovePacket())
	{
		return;
	}
	
	// TODO: 시간이 되었거나 변동 사항이 있어 패킷을 보내야 하는 경우 이동 패킷 전송
	//		 Movable에서 boolean IsMoveDirty 같은 걸 만들어서 위치나 회전이 변경되었는지 체크하는 방식으로 하는 게 좋아보임
	if (SendMoveElapsed < SendMoveInterval)
	{
		return;
	}
	
	SendMoveElapsed = 0.0f;
		
	FMoveSyncData MoveData;
	if (!BuildMoveSyncData(MoveData))
	{
		return;
	}
		
	UNetworkGameInstanceSubsystem* NetworkGIS = GetNetworkGameInstanceSubsystem();
	if (NetworkGIS == nullptr)
	{
		return;
	}
		
	NetworkGIS->SendMove(MoveData);
}

void UNetworkMoveComponent::TickRemote(float DeltaTime)
{
	if (!CanApplyRemoteInterpolation())
	{
		return;
	}
	
	ApplyRemoteInterpolation(DeltaTime);
}

void UNetworkMoveComponent::TickServer(float DeltaTime)
{
	if (!CanApplyRemoteInterpolation())
	{
		return;
	}
	
	ApplyRemoteInterpolation(DeltaTime);
}

void UNetworkMoveComponent::ApplyRemoteInterpolation(float DeltaTime)
{
	AActor* Owner = GetOwner();
	IMovableNetworkEntityInterface* Movable = Cast<IMovableNetworkEntityInterface>(Owner);
	ACharacter* Character = Cast<ACharacter>(Owner);
	if (Owner == nullptr || Movable == nullptr || Character == nullptr)
	{
		return;
	}

	UCharacterMovementComponent* CMC = Character->GetCharacterMovement();
	if (CMC == nullptr)
	{
		return;
	}
	
	const FVector CurrentPosition = Movable->GetNetworkLocation();
	const float CurrentYaw = Movable->GetNetworkYaw();
	const float CurrentPitch = Movable->GetNetworkPitch();
	
	const bool bCloseEnoughPosition = IsCloseEnoughPosition(CurrentPosition);
	const bool bCloseEnoughYaw = IsCloseEnoughYaw(CurrentYaw);
	const bool bCloseEnoughPitch = IsCloseEnoughPitch(CurrentPitch);
	
	if (bCloseEnoughPosition && bCloseEnoughYaw && bCloseEnoughPitch)
	{
		SnapToTarget();
		return;
	}
	
	InterpElapsed += DeltaTime;
	const float Alpha = FMath::Clamp(InterpElapsed / InterpDuration, 0.0f, 1.0f);
	
	const FVector NewPosition = FMath::Lerp(InterpStartPosition, InterpTargetPosition, Alpha);
	if (DeltaTime > 0.0f)
	{
		MoveStep = (NewPosition - CurrentPosition) / DeltaTime;
		
		CMC->Velocity = MoveStep;
		
		if (!MoveStep.IsNearlyZero())
		{
			FTimeThiefMovementAccessor::SetAcceleration(CMC, MoveStep.GetSafeNormal() * CMC->GetMaxAcceleration());
		}
		else
		{
			FTimeThiefMovementAccessor::SetAcceleration(CMC, FVector::ZeroVector);
		}

		Movable->SetNetworkLocation(NewPosition);
	}
	
	const float DeltaYaw = FMath::FindDeltaAngleDegrees(CurrentYaw, TargetYaw);
	const float NewYaw = FRotator::NormalizeAxis(StartYaw + DeltaYaw * Alpha);
	Movable->SetNetworkYaw(NewYaw);
	
	const float DeltaPitch = FMath::FindDeltaAngleDegrees(StartPitch, TargetPitch);
	const float NewPitch = FRotator::NormalizeAxis(StartPitch + DeltaPitch * Alpha);
	Movable->SetNetworkPitch(NewPitch);
	
	const float DeltaSpeed = FMath::Lerp(StartSpeed, TargetSpeed, Alpha);
	Movable->SetNetworkSpeed(DeltaSpeed);
	
}

void UNetworkMoveComponent::SnapToTarget()
{
	AActor* Owner = GetOwner();
	IMovableNetworkEntityInterface* Movable = Cast<IMovableNetworkEntityInterface>(Owner);
	ACharacter* Character = Cast<ACharacter>(Owner);
	if (Owner == nullptr || Movable == nullptr)
	{
		return;
	}
	
	Movable->SetNetworkLocation(InterpTargetPosition);
	Movable->SetNetworkYaw(TargetYaw);
	Movable->SetNetworkPitch(TargetPitch);
	Movable->SetNetworkSpeed(TargetSpeed);
	
	if (Character && Character->GetCharacterMovement())
	{
		Character->GetCharacterMovement()->Velocity = FVector::ZeroVector;
		FTimeThiefMovementAccessor::SetAcceleration(Character->GetCharacterMovement(), FVector::ZeroVector);
	}
}

bool UNetworkMoveComponent::CanSendMovePacket() const
{
	if (NetworkEntityComponent == nullptr)
	{
		return false;
	}
	
	if (!NetworkEntityComponent->IsLocalControlled())
	{
		return false;
	}
	
	if (!NetworkEntityComponent->IsValidEntity())
	{
		return false;
	}
	
	const UNetworkGameInstanceSubsystem* NetworkGIS = NGIS;
	if (NetworkGIS == nullptr)
	{
		return false;
	}
	
	return NetworkGIS->CanSendGameplayPacket();
}

bool UNetworkMoveComponent::CanApplyRemoteInterpolation() const
{
	if (NetworkEntityComponent == nullptr)
	{
		return false;
	}
	
	if (!NetworkEntityComponent->IsValidEntity())
	{
		return false;
	}
	
	if (NetworkEntityComponent->IsLocalControlled())
	{
		return false;
	}
	
	const UNetworkGameInstanceSubsystem* NetworkGIS = NGIS;
	if (NetworkGIS == nullptr)
	{
		return false;
	}
	
	return NetworkGIS->IsConnected();
}

IMovableNetworkEntityInterface* UNetworkMoveComponent::GetMovableOwner() const
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr)
	{
		return nullptr;
	}
	
	return Cast<IMovableNetworkEntityInterface>(Owner);
}

UNetworkGameInstanceSubsystem* UNetworkMoveComponent::GetNetworkGameInstanceSubsystem()
{
	if (NGIS)
	{
		return NGIS;
	}
	
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}
	
	UGameInstance* GameInstance = World->GetGameInstance();
	if (GameInstance == nullptr)
	{
		return nullptr;
	}
	
	NGIS = GameInstance->GetSubsystem<UNetworkGameInstanceSubsystem>();
	return NGIS;
}

