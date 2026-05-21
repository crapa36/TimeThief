#include "NetworkMoveComponent.h"

#include "NetworkGameInstanceSubsystem.h"
#include "Character/TimeThiefNetworkCharacterBase.h"
#include "Network/NetworkEntityComponent.h"
#include "Network/MovableNetworkEntityInterface.h"
#include "Network/State/NetworkControlType.h"
#include "Network/State/NetworkEntityState.h"
#include "Network/State/MoveSyncData.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "State/NetworkActionTypes.h"

namespace
{
	static constexpr float MinSyntheticDeltaTime = 1.0f / 120.0f;

	static void ApplyMovementModeIfNeeded(UCharacterMovementComponent* CMC, EMovementMode DesiredMode)
	{
		if (!CMC || DesiredMode == MOVE_None)
		{
			return;
		}

		if (CMC->MovementMode != DesiredMode)
		{
			CMC->SetMovementMode(DesiredMode);
		}
	}
}

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

	if (NetworkEntityComponent->IsLocalControlled())
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
	const float CurrentCharYaw = Movable->GetNetworkCharYaw();
	const float CurrentAimYaw = Movable->GetNetworkAimYaw();
	const float CurrentAimPitch = Movable->GetNetworkAimPitch();
	
	InterpStartPosition = CurrentPosition;
	InterpTargetPosition = EntityState.Position;
	
	StartCharYaw = CurrentCharYaw;
	TargetCharYaw = EntityState.CharYaw;
	StartAimYaw = CurrentAimYaw;
	TargetAimYaw = EntityState.AimYaw;
	StartAimPitch = CurrentAimPitch;
	TargetAimPitch = EntityState.AimPitch;
	
	TargetVelocity = FVector2D(EntityState.Velocity.X, EntityState.Velocity.Y);
	
	RecentMovementMode = EntityState.MovementMode;
	Movable->SetNetworkMovementMode(RecentMovementMode);

	InterpElapsed = 0.0f;
	// UE_LOG(LogTemp, Log, TEXT("[Network] Received Packet - EntityID: %u, AimYaw: %f, AimPitch: %f"), NetworkEntityComponent->GetEntityId(), EntityState.AimYaw, EntityState.AimPitch);
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

void UNetworkMoveComponent::HandleActionEvent(const FNetworkActionEvent& ActionEvent)
{
	const bool bIsLocalControlled = NetworkEntityComponent && NetworkEntityComponent->IsLocalControlled();
	if (!bIsLocalControlled)
	{
		ApplyActionEvent(ActionEvent);	// NMC에서 CMC를 직접 다루기가 부담스럽다 Broadcast만 하자
		OnRemoteActionNotify.Broadcast(ActionEvent);
	}
}

void UNetworkMoveComponent::ApplyActionEvent(const FNetworkActionEvent& ActionEvent)
{
	switch (ActionEvent.ActionType)
	{
	case ENetworkActionType::Jump:
		ApplyJumpAction(ActionEvent.Phase);
		break;

	case ENetworkActionType::Crouch:
		ApplyCrouchAction(ActionEvent.Phase);
		break;

	default:
		break;
	}
}

void UNetworkMoveComponent::ApplyJumpAction(ENetworkActionPhase Phase)
{
	auto Character = GetOwner<ATimeThiefNetworkCharacterBase>();
	if (!Character)
	{
		return;
	}

	UCharacterMovementComponent* CMC = Character->GetCharacterMovement();
	if (!CMC)
	{
		return;
	}

	switch (Phase)
	{
	case ENetworkActionPhase::Start:
		ApplyMovementModeIfNeeded(CMC, MOVE_Falling);
		Character->bIsJumping = true;
		break;
		
	case ENetworkActionPhase::Double:
		Character->DoubleJump();
		break;

	case ENetworkActionPhase::Land:
		{
			EMovementMode DesiredMode = RecentMovementMode;
			if (DesiredMode == MOVE_Falling || DesiredMode == MOVE_None)
			{
				DesiredMode = MOVE_Walking;
			}
			ApplyMovementModeIfNeeded(CMC, DesiredMode);
			Character->bIsJumping = false;
			break;
		}

	default:
		break;
	}
}

void UNetworkMoveComponent::ApplyCrouchAction(ENetworkActionPhase Phase)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return;
	}

	switch (Phase)
	{
	case ENetworkActionPhase::Start:
		if (!Character->bIsCrouched)
		{
			Character->Crouch(true);
		}
		break;

	case ENetworkActionPhase::End:
	case ENetworkActionPhase::Land:
		if (Character->bIsCrouched)
		{
			Character->UnCrouch(true);
		}
		break;

	default:
		break;
	}
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
	OutSyncData.CharYaw = Movable->GetNetworkCharYaw();
	OutSyncData.AimYaw = Movable->GetNetworkAimYaw();
	OutSyncData.AimPitch = Movable->GetNetworkAimPitch();
	const FVector2D& Velo2D = Movable->GetLocalControlVelocity2D();
	OutSyncData.Velocity = FVector(Velo2D.X, Velo2D.Y, 0.0f);
	OutSyncData.MovementMode = Movable->GetLocalControlMovementMode();
	
	return true;
}

bool UNetworkMoveComponent::IsCloseEnoughPosition(const FVector& CurrentPosition) const
{
	return FVector::DistSquared(CurrentPosition, InterpTargetPosition) <= FMath::Square(PositionTolerance);
}

bool UNetworkMoveComponent::IsCloseEnoughCharYaw(float CurrentCharYaw) const
{
	return FMath::Abs(FMath::FindDeltaAngleDegrees(CurrentCharYaw, TargetCharYaw)) <= RotationTolerance;
}

bool UNetworkMoveComponent::IsCloseEnoughAimYaw(float CurrentAimYaw) const
{
	return FMath::Abs(FMath::FindDeltaAngleDegrees(CurrentAimYaw, TargetAimYaw)) <= RotationTolerance;
}

bool UNetworkMoveComponent::IsCloseEnoughAimPitch(float CurrentAimPitch) const
{
	return  FMath::Abs(FMath::FindDeltaAngleDegrees(CurrentAimPitch, TargetAimPitch)) <= RotationTolerance;
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
	float CurrentAimYaw = Movable->GetLocalControlAimYaw();
	Movable->SetNetworkAimYaw(CurrentAimYaw);
	float CurrentAimPitch = Movable->GetLocalControlAimPitch();
	Movable->SetNetworkAimPitch(CurrentAimPitch);
	FVector2D CurrentVelocity = Movable->GetLocalControlVelocity2D();
	Movable->SetNetworkVelocity2D(CurrentVelocity);
	EMovementMode CurrentMovementMode = Movable->GetLocalControlMovementMode();
	Movable->SetNetworkMovementMode(CurrentMovementMode);
	
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
	
	ApplyServerInterpolation(DeltaTime);
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
	const float CurrentYaw = Movable->GetNetworkCharYaw();
	
	const bool bCloseEnoughPosition = IsCloseEnoughPosition(CurrentPosition);
	const bool bCloseEnoughYaw = IsCloseEnoughCharYaw(CurrentYaw);
	const bool bCloseEnoughAimYaw = IsCloseEnoughAimYaw(Movable->GetNetworkAimYaw());
	const bool bCloseEnoughPitch = IsCloseEnoughAimPitch(Movable->GetNetworkAimPitch());
	
	if (bCloseEnoughPosition && bCloseEnoughYaw && bCloseEnoughAimYaw && bCloseEnoughPitch)
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

		FVector NewVelocity = MoveStep;
		if (CMC->MovementMode == MOVE_Falling)
		{
			// Keep vertical momentum during jump/fall. Network packets currently carry only planar velocity.
			NewVelocity.Z = CMC->Velocity.Z;
		}
		else
		{
			NewVelocity.Z = 0.0f;
		}
		CMC->Velocity = NewVelocity;
		CMC->RequestDirectMove(NewVelocity, false);

		Movable->SetNetworkLocation(NewPosition);
	}
	
	const float DeltaYaw = FMath::FindDeltaAngleDegrees(CurrentYaw, TargetCharYaw);
	const float NewYaw = FRotator::NormalizeAxis(StartCharYaw + DeltaYaw * Alpha);
	Movable->SetNetworkCharYaw(NewYaw);
	
	const float DeltaAimYaw = FMath::FindDeltaAngleDegrees(Movable->GetNetworkAimYaw(), TargetAimYaw);
	const float NewAimYaw = FRotator::NormalizeAxis(StartAimYaw + DeltaAimYaw * Alpha);
	Movable->SetNetworkAimYaw(NewAimYaw);
	
	const float DeltaPitch = FMath::FindDeltaAngleDegrees(Movable->GetNetworkAimPitch(), TargetAimPitch);
	const float NewPitch = FRotator::NormalizeAxis(StartAimPitch + DeltaPitch * Alpha);
	Movable->SetNetworkAimPitch(NewPitch);

	const FVector2D NewVelocity = BuildSyntheticVelocity2D(CurrentPosition, NewPosition, DeltaTime, TargetVelocity);
	Movable->SetNetworkVelocity2D(NewVelocity);
}

void UNetworkMoveComponent::ApplyServerInterpolation(float DeltaTime)
{
	AActor* Owner = GetOwner();
	IMovableNetworkEntityInterface* Movable = Cast<IMovableNetworkEntityInterface>(Owner);
	APawn* Pawn = Cast<APawn>(Owner);

	if (Owner == nullptr || Movable == nullptr || Pawn == nullptr)
	{
		return;
	}

	const FVector CurrentPosition = Movable->GetNetworkLocation();
	const float CurrentYaw = Movable->GetNetworkCharYaw();

	const bool bCloseEnoughPosition = IsCloseEnoughPosition(CurrentPosition);
	const bool bCloseEnoughYaw = IsCloseEnoughCharYaw(CurrentYaw);

	if (bCloseEnoughPosition && bCloseEnoughYaw)
	{
		SnapToTarget();
		return;
	}

	InterpElapsed += DeltaTime;

	const float Alpha = FMath::Clamp(InterpElapsed / 0.2f, 0.0f, 1.0f);
	// 또는 기존 값 사용
	// const float Alpha = FMath::Clamp(InterpElapsed / InterpDuration, 0.0f, 1.0f);

	const FVector NewPosition = FMath::Lerp(
		InterpStartPosition,
		InterpTargetPosition,
		Alpha
	);

	if (DeltaTime > 0.0f)
	{
		MoveStep = (NewPosition - CurrentPosition) / DeltaTime;

		Movable->SetNetworkLocation(NewPosition);

		const FVector2D NewVelocity = BuildSyntheticVelocity2D(
			CurrentPosition,
			NewPosition,
			DeltaTime,
			TargetVelocity
		);

		Movable->SetNetworkVelocity2D(NewVelocity);
	}

	// Character Yaw
	const float DeltaYaw = FMath::FindDeltaAngleDegrees(StartCharYaw, TargetCharYaw);
	const float NewYaw = FRotator::NormalizeAxis(StartCharYaw + DeltaYaw * Alpha);
	Movable->SetNetworkCharYaw(NewYaw);
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

	const FVector CurrentPosition = Movable->GetNetworkLocation();
	const float RemainingDeltaTime = FMath::Max(InterpDuration - InterpElapsed, MinSyntheticDeltaTime);
	const FVector2D SnapVelocity2D = BuildSyntheticVelocity2D(CurrentPosition, InterpTargetPosition, RemainingDeltaTime, TargetVelocity);
	
	Movable->SetNetworkLocation(InterpTargetPosition);
	Movable->SetNetworkCharYaw(TargetCharYaw);
	Movable->SetNetworkAimYaw(TargetAimYaw);
	Movable->SetNetworkAimPitch(TargetAimPitch);
	Movable->SetNetworkVelocity2D(SnapVelocity2D);
	MoveStep = FVector(SnapVelocity2D.X, SnapVelocity2D.Y, 0.0f);
	
	if (Character && Character->GetCharacterMovement())
	{
		UCharacterMovementComponent* CMC = Character->GetCharacterMovement();
		ApplyMovementModeIfNeeded(CMC, RecentMovementMode);

		FVector SnapVelocity(SnapVelocity2D.X, SnapVelocity2D.Y, 0.0f);
		if (CMC->MovementMode == MOVE_Falling)
		{
			SnapVelocity.Z = CMC->Velocity.Z;
		}
		CMC->Velocity = SnapVelocity;
		CMC->RequestDirectMove(SnapVelocity, false);
	}
}

FVector2D UNetworkMoveComponent::BuildSyntheticVelocity2D(const FVector& FromPosition, const FVector& ToPosition, float DeltaSeconds, const FVector2D& FallbackVelocity) const
{
	const FVector Delta = ToPosition - FromPosition;
	const FVector DeltaPlanar(Delta.X, Delta.Y, 0.0f);
	if (DeltaPlanar.SizeSquared() < FMath::Square(MinSyntheticDisplacementCm))
	{
		return FallbackVelocity;
	}

	const float SafeDeltaSeconds = FMath::Max(DeltaSeconds, MinSyntheticDeltaTime);
	const FVector SyntheticVelocity3D = DeltaPlanar / SafeDeltaSeconds;
	return FVector2D(SyntheticVelocity3D.X, SyntheticVelocity3D.Y);
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