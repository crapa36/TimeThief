#include "Components/Wire/TimeThiefWireComponent.h"
#include "TimeThiefGameplayTags.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "DrawDebugHelpers.h"

DEFINE_LOG_CATEGORY(LogWire);

UTimeThiefWireComponent::UTimeThiefWireComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UTimeThiefWireComponent::BeginPlay()
{
	Super::BeginPlay();

	CachedCharacter = GetPawn<ACharacter>();
	if (IsValid(CachedCharacter))
	{
		CachedMovementComponent = CachedCharacter->GetCharacterMovement();
	}

	if (WireCollisionObjectTypes.IsEmpty())
	{
		WireCollisionObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));
		WireCollisionObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));
	}
}

void UTimeThiefWireComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateCooldown(DeltaTime);

	switch (CurrentState)
	{
	case EWireState::Firing:
		UpdateFiringAnchor(DeltaTime);
		DrawWireLine();
		break;
	case EWireState::Attached:
		UpdateAttachedWire(DeltaTime);
		DrawWireLine();
		break;
	default:
		break;
	}

	if (!ShouldTickComponent())
	{
		SetComponentTickEnabled(false);
	}
}

bool UTimeThiefWireComponent::ShouldTickComponent() const
{
	return CurrentState != EWireState::Idle || CooldownRemaining > 0.0f;
}

void UTimeThiefWireComponent::UpdateCooldown(float DeltaTime)
{
	if (CooldownRemaining > 0.0f)
	{
		CooldownRemaining = FMath::Max(0.0f, CooldownRemaining - DeltaTime);
	}
}

bool UTimeThiefWireComponent::CanFireWire() const
{
	return CurrentState == EWireState::Idle && CooldownRemaining <= 0.0f;
}

void UTimeThiefWireComponent::FireWire()
{
	if (!CanFireWire()) return;

	FireDirection = GetAimDirection().GetSafeNormal();
	AnchorPoint = GetWireStartLocation();
	CurrentFireDistance = 0.0f;
	StuckCheckTimer = 0.0f;
	InputAgainstWireTimer = 0.0f;
	GroundCheckTimer = 0.0f;

	SetComponentTickEnabled(true);
	SetWireState(EWireState::Firing);
}

void UTimeThiefWireComponent::ReleaseWire()
{
	if (CurrentState == EWireState::Idle) return;

	if (CurrentState == EWireState::Attached)
	{
		if (IsValid(CachedMovementComponent))
		{
			CachedMovementComponent->GravityScale = CachedGravityScale;
			CachedMovementComponent->AirControl = CachedAirControl;
		}
	}

	CooldownRemaining = WireCooldown;
	AnchorPoint = FVector::ZeroVector;
	FireDirection = FVector::ZeroVector;
	CurrentFireDistance = 0.0f;
	AttachedWireLength = 0.0f;

	SetWireState(EWireState::Idle);
}

void UTimeThiefWireComponent::SetWireState(EWireState NewState)
{
	if (CurrentState == NewState) return;
	
	const EWireState OldState = CurrentState;
	CurrentState = NewState;
	OnWireStateChanged.Broadcast(OldState, NewState);
}

void UTimeThiefWireComponent::UpdateFiringAnchor(float DeltaTime)
{
	const FVector PreviousAnchorPoint = AnchorPoint;
	const float MoveDistance = WireFireSpeed * DeltaTime;
	CurrentFireDistance += MoveDistance;
	AnchorPoint += FireDirection * MoveDistance;

	if (CurrentFireDistance > MaxWireLength)
	{
		AnchorPoint = GetWireStartLocation() + FireDirection * MaxWireLength;
	}

	FHitResult HitResult;
	if (CheckAnchorCollision(PreviousAnchorPoint, AnchorPoint, HitResult))
	{
		AnchorPoint = HitResult.ImpactPoint;
		AttachedWireLength = FVector::Dist(GetWireStartLocation(), AnchorPoint);
		OnAnchorAttached();
		SetWireState(EWireState::Attached);
		OnWireAttached.Broadcast(AnchorPoint);
		return;
	}
	
	if (CurrentFireDistance >= MaxWireLength)
	{
		ReleaseWire();
	}
}

void UTimeThiefWireComponent::OnAnchorAttached()
{
	if (!IsValid(CachedMovementComponent)) return;

	CachedGravityScale = CachedMovementComponent->GravityScale;
	CachedAirControl = CachedMovementComponent->AirControl;

	CachedMovementComponent->GravityScale = GravityMultiplierOnWire;
	CachedMovementComponent->AirControl = AirControlOnWire;
	CachedMovementComponent->SetMovementMode(MOVE_Falling);

	const FVector WireDirection = (AnchorPoint - GetWireStartLocation()).GetSafeNormal();
	const float VelocityTowardAnchor = FVector::DotProduct(CachedMovementComponent->Velocity, WireDirection);
	if (VelocityTowardAnchor < 0)
	{
		CachedMovementComponent->Velocity -= WireDirection * VelocityTowardAnchor;
	}
}

void UTimeThiefWireComponent::UpdateAttachedWire(float DeltaTime)
{
	if (!IsValid(CachedCharacter) || !IsValid(CachedMovementComponent))
	{
		ReleaseWire();
		return;
	}

	if (!CachedMovementComponent->IsFalling())
	{
		CachedMovementComponent->SetMovementMode(MOVE_Falling);
	}

	const FVector ToAnchor = AnchorPoint - GetWireStartLocation();
	CurrentWireDistance = ToAnchor.Size();
	CurrentWireDirection = ToAnchor.GetSafeNormal();

	if (CurrentWireDistance <= ArrivalDistance || ShouldRelease(DeltaTime))
	{
		ReleaseWire();
		return;
	}

	if (CurrentWireDistance < AttachedWireLength - WireLengthUpdateTolerance)
	{
		AttachedWireLength = CurrentWireDistance;
	}

	const FVector ConstraintForce = CalculateWireConstraintForce();
	const FVector PullForce = CalculatePullForce();
	const FVector InputForce = CalculateSwingInputForce();
	const FVector DragForce = CalculateDragForce();

	const FVector TotalForce = ConstraintForce + PullForce + InputForce + DragForce;
	
	CachedMovementComponent->AddForce(TotalForce);
}

float UTimeThiefWireComponent::GetCurrentWireLength() const
{
	if (CurrentState == EWireState::Idle) return 0.0f;
	return FVector::Dist(GetWireStartLocation(), AnchorPoint);
}

void UTimeThiefWireComponent::HandleInputPressed(FGameplayTag InputTag)
{
	const FTimeThiefGameplayTags& Tags = FTimeThiefGameplayTags::Get();

	if (InputTag == Tags.InputTag_Action_Wire)
	{
		CurrentState == EWireState::Idle ? FireWire() : ReleaseWire();
	}
}

FVector UTimeThiefWireComponent::CalculateWireConstraintForce() const
{
	if (!IsValid(CachedMovementComponent)) return FVector::ZeroVector;

	if (CurrentWireDistance <= AttachedWireLength)
	{
		return FVector::ZeroVector;
	}

	const float Displacement = CurrentWireDistance - AttachedWireLength;
	const FVector SpringForce = CurrentWireDirection * (WireStiffness * Displacement);

	const FVector Velocity = CachedMovementComponent->Velocity;
	const float VelocityAlongWire = FVector::DotProduct(Velocity, CurrentWireDirection);
	const FVector DampingForce = -CurrentWireDirection * (WireDamping * VelocityAlongWire);

	return SpringForce + DampingForce;
}

FVector UTimeThiefWireComponent::CalculatePullForce() const
{
	if (MoveInput.IsNearlyZero())
	{
		return CurrentWireDirection * PullInForce;
	}

	return FVector::ZeroVector;
}

FVector UTimeThiefWireComponent::CalculateSwingInputForce() const
{
	if (!IsValid(CachedCharacter) || MoveInput.IsNearlyZero()) return FVector::ZeroVector;

	const APlayerController* PC = Cast<APlayerController>(CachedCharacter->GetController());
	if (!PC) return FVector::ZeroVector;

	const FRotator ControlRotation = PC->GetControlRotation();
	const FVector ForwardDir = FRotationMatrix(ControlRotation).GetUnitAxis(EAxis::X);
	const FVector RightDir = FRotationMatrix(ControlRotation).GetUnitAxis(EAxis::Y);
	
	const FVector InputDirection = (ForwardDir * MoveInput.Y + RightDir * MoveInput.X).GetSafeNormal();

	if (InputDirection.IsNearlyZero()) return FVector::ZeroVector;

	const float InputAlongWire = FVector::DotProduct(InputDirection, CurrentWireDirection);
	const FVector RadialInputComponent = CurrentWireDirection * InputAlongWire;

	const FVector TangentialInputDirection = (InputDirection - RadialInputComponent).GetSafeNormal();

	if (!TangentialInputDirection.IsNearlyZero())
	{
		return TangentialInputDirection * SwingInputForce;
	}

	return FVector::ZeroVector;
}

FVector UTimeThiefWireComponent::CalculateDragForce() const
{
	if (!IsValid(CachedMovementComponent)) return FVector::ZeroVector;

	const FVector Velocity = CachedMovementComponent->Velocity;
	const float SpeedSq = Velocity.SizeSquared();
	
	if (SpeedSq < KINDA_SMALL_NUMBER) return FVector::ZeroVector;

	const float MaxSpeed = CachedMovementComponent->MaxWalkSpeed * MaxSwingSpeedMultiplier;
	if (Velocity.Size() > MaxSpeed)
	{
		return -Velocity.GetSafeNormal() * (SpeedSq * SwingDragCoefficient);
	}

	return FVector::ZeroVector;
}

bool UTimeThiefWireComponent::ShouldRelease(float DeltaTime)
{
	return IsStuck(DeltaTime) || IsPushingAgainstWire(DeltaTime) || IsOnGroundTooLong(DeltaTime) || IsWireSnapping();
}

bool UTimeThiefWireComponent::IsStuck(float DeltaTime)
{
	if (!IsValid(CachedMovementComponent)) return true;

	if (CachedMovementComponent->Velocity.Size() < StuckSpeedThreshold)
	{
		StuckCheckTimer += DeltaTime;
		if (StuckCheckTimer >= StuckCheckDelay)
		{
			return true;
		}
	}
	else
	{
		StuckCheckTimer = 0.0f;
	}
	return false;
}

bool UTimeThiefWireComponent::IsPushingAgainstWire(float DeltaTime)
{
	const bool bWireTight = CurrentWireDistance >= AttachedWireLength * WireTightThreshold;
	if (!bWireTight || MoveInput.IsNearlyZero() || !IsValid(CachedCharacter))
	{
		InputAgainstWireTimer = 0.0f;
		return false;
	}

	const APlayerController* PC = Cast<APlayerController>(CachedCharacter->GetController());
	if (!PC) return false;

	const FRotator ControlRotation = PC->GetControlRotation();
	const FVector ForwardDir = FRotationMatrix(ControlRotation).GetUnitAxis(EAxis::X);
	const FVector RightDir = FRotationMatrix(ControlRotation).GetUnitAxis(EAxis::Y);
	const FVector InputDirection = (ForwardDir * MoveInput.Y + RightDir * MoveInput.X).GetSafeNormal();

	const float InputDotWire = FVector::DotProduct(InputDirection, CurrentWireDirection);
	
	if (InputDotWire < -InputAgainstWireThreshold)
	{
		InputAgainstWireTimer += DeltaTime;
		if (InputAgainstWireTimer >= InputAgainstWireDelay)
		{
			return true;
		}
	}
	else
	{
		InputAgainstWireTimer = 0.0f;
	}

	return false;
}

bool UTimeThiefWireComponent::IsOnGroundTooLong(float DeltaTime)
{
	if (!IsValid(CachedMovementComponent)) return false;

	FHitResult FloorHit;
	const bool bOnGround = CachedMovementComponent->CurrentFloor.bBlockingHit;

	if (bOnGround)
	{
		GroundCheckTimer += DeltaTime;
		if (GroundCheckTimer >= MaxGroundTime)
		{
			return true;
		}
	}
	else
	{
		GroundCheckTimer = 0.0f;
	}
	return false;
}

bool UTimeThiefWireComponent::IsWireSnapping() const
{
	if (!IsValid(CachedMovementComponent)) return false;

	const FVector Velocity = CachedMovementComponent->Velocity;
	const float Speed = Velocity.Size();

	if (Speed < WireBreakSpeedThreshold)
	{
		return false;
	}

	const FVector VelocityDir = Velocity.GetSafeNormal();
	const float DotProduct = FVector::DotProduct(VelocityDir, CurrentWireDirection);

	if (DotProduct < WireBreakAngleThreshold)
	{
		return true;
	}

	return false;
}

FVector UTimeThiefWireComponent::GetAimDirection() const
{
	if (!IsValid(CachedCharacter)) return FVector::ForwardVector;

	const APlayerController* PC = Cast<APlayerController>(CachedCharacter->GetController());
	if (!PC) return CachedCharacter->GetActorForwardVector();

	FVector CameraLocation;
	FRotator CameraRotation;
	PC->GetPlayerViewPoint(CameraLocation, CameraRotation);
	return CameraRotation.Vector();
}

FVector UTimeThiefWireComponent::GetWireStartLocation() const
{
	if (!IsValid(CachedCharacter)) return GetOwner()->GetActorLocation();

	FVector Location = CachedCharacter->GetActorLocation();
	Location.Z += WireStartZOffset;
	return Location;
}

FVector UTimeThiefWireComponent::GetTangentVelocity(const FVector& Velocity, const FVector& WireDirection) const
{
	const float VelocityAlongWire = FVector::DotProduct(Velocity, WireDirection);
	return Velocity - WireDirection * VelocityAlongWire;
}

bool UTimeThiefWireComponent::CheckAnchorCollision(const FVector& Start, const FVector& End, FHitResult& OutHit)
{
	UWorld* World = GetWorld();
	if (!World) return false;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetOwner());

	return World->LineTraceSingleByObjectType(
		OutHit, Start, End,
		FCollisionObjectQueryParams(WireCollisionObjectTypes),
		QueryParams
	);
}

void UTimeThiefWireComponent::DrawWireLine() const
{
#if WITH_EDITOR
	const UWorld* World = GetWorld();
	if (!World) return;

	DrawDebugLine(World, GetWireStartLocation(), AnchorPoint, DebugWireColor, false, 0.0f, 0, DebugWireThickness);
#endif
}
