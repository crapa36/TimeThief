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

	CachedMovementComponent->GravityScale = WireGravityScale;
	CachedMovementComponent->AirControl = 1.0f;
	CachedMovementComponent->SetMovementMode(MOVE_Falling);

	const FVector WireDirection = (AnchorPoint - GetWireStartLocation()).GetSafeNormal();
	const float VelocityTowardAnchor = FVector::DotProduct(CachedMovementComponent->Velocity, WireDirection);
	if (VelocityTowardAnchor < 0)
	{
		CachedMovementComponent->Velocity -= WireDirection * VelocityTowardAnchor;
	}
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

void UTimeThiefWireComponent::UpdateAttachedWire(float DeltaTime)
{
	if (!IsValid(CachedCharacter) || !IsValid(CachedMovementComponent))
	{
		ReleaseWire();
		return;
	}
	
	if (CachedMovementComponent->IsMovingOnGround())
	{
		CachedMovementComponent->SetMovementMode(MOVE_Falling);
	}

	const FVector WireStartLocation = GetWireStartLocation();
	const FVector ToAnchor = AnchorPoint - WireStartLocation;
	const float CurrentDistance = ToAnchor.Size();
	const FVector WireDirection = ToAnchor.GetSafeNormal();

	if (CurrentDistance <= ArrivalDistance)
	{
		ReleaseWire();
		return;
	}

	if (ShouldReleaseByObstruction(WireDirection, CurrentDistance, DeltaTime))
	{
		ReleaseWire();
		return;
	}

	// Shorten the wire to pull the player
	const float TangentSpeed = GetTangentVelocity(CachedMovementComponent->Velocity, WireDirection).Size();
	const float PullAmount = FMath::Max(0.f, PullSpeed - TangentSpeed);
	AttachedWireLength = FMath::Max(ArrivalDistance, AttachedWireLength - PullAmount * DeltaTime);

	ApplyPendulumPhysics(WireDirection, DeltaTime);
	ConstrainToWireLength();
}

void UTimeThiefWireComponent::ApplyPendulumPhysics(const FVector& WireDirection, float DeltaTime)
{
	if (!IsValid(CachedMovementComponent)) return;

	FVector Velocity = CachedMovementComponent->Velocity;

	const float WorldGravity = FMath::Abs(GetWorld()->GetGravityZ());
	const FVector GravityVector(0.0f, 0.0f, -WorldGravity);
	
	const float GravityAlongWire = FVector::DotProduct(GravityVector, WireDirection);
	const FVector GravityTangent = GravityVector - WireDirection * GravityAlongWire;
	Velocity += GravityTangent * DeltaTime;

	if (!MoveInput.IsNearlyZero())
	{
		Velocity += GetPlayerInputAcceleration(DeltaTime);
	}

	CachedMovementComponent->Velocity = Velocity;
}

FVector UTimeThiefWireComponent::GetPlayerInputAcceleration(float DeltaTime) const
{
	if (!IsValid(CachedCharacter) || MoveInput.IsNearlyZero()) return FVector::ZeroVector;

	const APlayerController* PC = Cast<APlayerController>(CachedCharacter->GetController());
	if (!PC) return FVector::ZeroVector;

	const FRotator ControlRotation = PC->GetControlRotation();
	const FVector ForwardDir = FRotationMatrix(ControlRotation).GetUnitAxis(EAxis::X);
	const FVector RightDir = FRotationMatrix(ControlRotation).GetUnitAxis(EAxis::Y);
	
	const FVector InputDirection = (ForwardDir * MoveInput.Y + RightDir * MoveInput.X).GetSafeNormal();

	if (InputDirection.IsNearlyZero()) return FVector::ZeroVector;

	return InputDirection * SwingInputAcceleration * DeltaTime;
}

FVector UTimeThiefWireComponent::GetTangentVelocity(const FVector& Velocity, const FVector& WireDirection) const
{
	const float VelocityAlongWire = FVector::DotProduct(Velocity, WireDirection);
	return Velocity - WireDirection * VelocityAlongWire;
}

bool UTimeThiefWireComponent::ShouldReleaseByObstruction(const FVector& WireDirection, float CurrentDistance, float DeltaTime)
{
	if (!IsValid(CachedMovementComponent)) return true;

	const float CurrentSpeed = CachedMovementComponent->Velocity.Size();
	
	if (CurrentSpeed < StuckSpeedThreshold)
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

	const bool bWireTight = CurrentDistance >= AttachedWireLength * WireTightThreshold;
	
	if (bWireTight && !MoveInput.IsNearlyZero())
	{
		if (!IsValid(CachedCharacter)) return false;

		const APlayerController* PC = Cast<APlayerController>(CachedCharacter->GetController());
		if (!PC) return false;

		const FRotator ControlRotation = PC->GetControlRotation();
		const FVector ForwardDir = FRotationMatrix(ControlRotation).GetUnitAxis(EAxis::X);
		const FVector RightDir = FRotationMatrix(ControlRotation).GetUnitAxis(EAxis::Y);
		const FVector InputDirection = (ForwardDir * MoveInput.Y + RightDir * MoveInput.X).GetSafeNormal();

		const float InputDotWire = FVector::DotProduct(InputDirection, WireDirection);
		
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
	}
	else
	{
		InputAgainstWireTimer = 0.0f;
	}

	return false;
}

void UTimeThiefWireComponent::ConstrainToWireLength()
{
	if (!IsValid(CachedCharacter) || !IsValid(CachedMovementComponent) || AttachedWireLength <= 0.0f) return;

	const FVector WireStartLocation = GetWireStartLocation();
	const FVector ToAnchor = AnchorPoint - WireStartLocation;
	const float CurrentDistance = ToAnchor.Size();

	if (CurrentDistance <= AttachedWireLength) return;

	const FVector WireDirection = ToAnchor.GetSafeNormal();
	const FVector ConstrainedLocation = AnchorPoint - WireDirection * AttachedWireLength;
	
	FVector NewActorLocation = ConstrainedLocation;
	NewActorLocation.Z -= WireStartZOffset;
	
	CachedCharacter->SetActorLocation(NewActorLocation, true, nullptr, ETeleportType::TeleportPhysics);

	FVector Velocity = CachedMovementComponent->Velocity;
	const float VelocityAwayFromAnchor = FVector::DotProduct(Velocity, -WireDirection);
	if (VelocityAwayFromAnchor > 0)
	{
		CachedMovementComponent->Velocity = Velocity + WireDirection * VelocityAwayFromAnchor;
	}
}
