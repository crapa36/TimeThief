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
		ACharacter* Character = GetPawn<ACharacter>();
		if (Character)
		{
			if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
			{
				Movement->GravityScale = CachedGravityScale;
				Movement->AirControl = CachedAirControl;
			}
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
	const ACharacter* Character = GetPawn<ACharacter>();
	if (!Character) return FVector::ForwardVector;

	const APlayerController* PC = Cast<APlayerController>(Character->GetController());
	if (!PC) return Character->GetActorForwardVector();

	FVector CameraLocation;
	FRotator CameraRotation;
	PC->GetPlayerViewPoint(CameraLocation, CameraRotation);
	return CameraRotation.Vector();
}

FVector UTimeThiefWireComponent::GetWireStartLocation() const
{
	const ACharacter* Character = GetPawn<ACharacter>();
	if (!Character) return GetOwner()->GetActorLocation();

	FVector Location = Character->GetActorLocation();
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
	ACharacter* Character = GetPawn<ACharacter>();
	if (!Character) return;

	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	if (!Movement) return;

	CachedGravityScale = Movement->GravityScale;
	CachedAirControl = Movement->AirControl;

	Movement->GravityScale = WireGravityScale;
	Movement->AirControl = 1.0f;
	Movement->SetMovementMode(MOVE_Falling);

	const FVector WireDirection = (AnchorPoint - GetWireStartLocation()).GetSafeNormal();
	const float VelocityTowardAnchor = FVector::DotProduct(Movement->Velocity, WireDirection);
	if (VelocityTowardAnchor < 0)
	{
		Movement->Velocity -= WireDirection * VelocityTowardAnchor;
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
	ACharacter* Character = GetPawn<ACharacter>();
	if (!Character) return;
	
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	if (!Movement) return;

	if (Movement->IsMovingOnGround())
	{
		Movement->SetMovementMode(MOVE_Falling);
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

	if (ShouldReleaseByObstruction(Movement, WireDirection, CurrentDistance, DeltaTime))
	{
		ReleaseWire();
		return;
	}

	ApplyPendulumPhysics(Movement, WireDirection, CurrentDistance, DeltaTime);
	ConstrainToWireLength(Character, Movement);
}

void UTimeThiefWireComponent::ApplyPendulumPhysics(UCharacterMovementComponent* Movement, const FVector& WireDirection, float WireLength, float DeltaTime)
{
	FVector Velocity = Movement->Velocity;

	const float WorldGravity = FMath::Abs(GetWorld()->GetGravityZ());
	const FVector GravityVector(0.0f, 0.0f, -WorldGravity);
	
	const float GravityAlongWire = FVector::DotProduct(GravityVector, WireDirection);
	const FVector GravityTangent = GravityVector - WireDirection * GravityAlongWire;
	Velocity += GravityTangent * DeltaTime;

	const FVector TangentVelocity = GetTangentVelocity(Velocity, WireDirection);
	const float TangentSpeed = TangentVelocity.Size();

	if (TangentSpeed < PullSpeed)
	{
		Velocity += WireDirection * (PullSpeed - TangentSpeed) * DeltaTime;
	}

	if (!MoveInput.IsNearlyZero())
	{
		Velocity += GetPlayerInputAcceleration(DeltaTime);
	}


	Movement->Velocity = Velocity;
}

FVector UTimeThiefWireComponent::GetPlayerInputAcceleration(float DeltaTime) const
{
	const ACharacter* Character = GetPawn<ACharacter>();
	if (!Character || MoveInput.IsNearlyZero()) return FVector::ZeroVector;

	const APlayerController* PC = Cast<APlayerController>(Character->GetController());
	if (!PC) return FVector::ZeroVector;

	FRotator ControlRotation = PC->GetControlRotation();
	ControlRotation.Pitch = 0.0f;
	ControlRotation.Roll = 0.0f;

	const FVector ForwardDir = ControlRotation.Vector();
	const FVector RightDir = FRotationMatrix(ControlRotation).GetScaledAxis(EAxis::Y);
	const FVector InputDirection = (ForwardDir * MoveInput.Y + RightDir * MoveInput.X).GetSafeNormal();

	if (InputDirection.IsNearlyZero()) return FVector::ZeroVector;

	return InputDirection * SwingInputAcceleration * DeltaTime;
}

FVector UTimeThiefWireComponent::GetTangentVelocity(const FVector& Velocity, const FVector& WireDirection) const
{
	const float VelocityAlongWire = FVector::DotProduct(Velocity, WireDirection);
	return Velocity - WireDirection * VelocityAlongWire;
}

bool UTimeThiefWireComponent::ShouldReleaseByObstruction(UCharacterMovementComponent* Movement, const FVector& WireDirection, float CurrentDistance, float DeltaTime)
{
	const float CurrentSpeed = Movement->Velocity.Size();
	
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
		const ACharacter* Character = GetPawn<ACharacter>();
		if (!Character) return false;

		const APlayerController* PC = Cast<APlayerController>(Character->GetController());
		if (!PC) return false;

		FRotator ControlRotation = PC->GetControlRotation();
		ControlRotation.Pitch = 0.0f;
		ControlRotation.Roll = 0.0f;

		const FVector ForwardDir = ControlRotation.Vector();
		const FVector RightDir = FRotationMatrix(ControlRotation).GetScaledAxis(EAxis::Y);
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

void UTimeThiefWireComponent::ConstrainToWireLength(ACharacter* Character, UCharacterMovementComponent* Movement)
{
	if (AttachedWireLength <= 0.0f) return;

	const FVector WireStartLocation = GetWireStartLocation();
	const FVector ToAnchor = AnchorPoint - WireStartLocation;
	const float CurrentDistance = ToAnchor.Size();

	if (CurrentDistance <= AttachedWireLength) return;

	const FVector WireDirection = ToAnchor.GetSafeNormal();
	const FVector ConstrainedLocation = AnchorPoint - WireDirection * AttachedWireLength;
	
	FVector NewActorLocation = ConstrainedLocation;
	NewActorLocation.Z -= WireStartZOffset;
	
	Character->SetActorLocation(NewActorLocation, true);

	FVector Velocity = Movement->Velocity;
	const float VelocityAwayFromAnchor = FVector::DotProduct(Velocity, -WireDirection);
	if (VelocityAwayFromAnchor > 0)
	{
		Movement->Velocity = Velocity + WireDirection * VelocityAwayFromAnchor;
	}
}

