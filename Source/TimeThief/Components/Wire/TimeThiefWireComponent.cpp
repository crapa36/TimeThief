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

void UTimeThiefWireComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseWire();
	Super::EndPlay(EndPlayReason);
}

void UTimeThiefWireComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateCooldown(DeltaTime);

	switch (CurrentState)
	{
	case EWireState::Firing:
		UpdateFiringAnchor(DeltaTime);
		break;
	case EWireState::Attached:
		UpdateAttachedWire(DeltaTime);
		break;
	default:
		break;
	}

	DrawWireLine();

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

	const FVector StartLocation = GetWireStartLocation();
	FVector TargetLocation;
	
	if (FindBestAnchorTarget(TargetLocation))
	{
		FireDirection = (TargetLocation - StartLocation).GetSafeNormal();
	}
	else
	{
		FireDirection = GetAimDirection();
	}

	AnchorPoint = StartLocation;
	CurrentFireDistance = 0.0f;
	StuckCheckTimer = 0.0f;
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

	SetWireState(EWireState::Idle);

	AnchorPoint = FVector::ZeroVector;
	FireDirection = FVector::ZeroVector;
	CurrentFireDistance = 0.0f;
	AttachedWireLength = 0.0f;
}

void UTimeThiefWireComponent::SetWireState(EWireState NewState)
{
	if (CurrentState == NewState) return;
	
	const EWireState OldState = CurrentState;
	CurrentState = NewState;
	OnWireStateChanged.Broadcast(OldState, NewState);

	if (CurrentState == EWireState::Attached)
	{
		OnAnchorAttached();
	}
}

void UTimeThiefWireComponent::UpdateFiringAnchor(float DeltaTime)
{
	const FVector StartLocation = GetWireStartLocation();
	const FVector PreviousAnchorPoint = AnchorPoint;
	const float MoveDistance = WireFireSpeed * DeltaTime;
	
	CurrentFireDistance += MoveDistance;
	AnchorPoint += FireDirection * MoveDistance;

	if (CurrentFireDistance > MaxWireLength)
	{
		AnchorPoint = StartLocation + FireDirection * MaxWireLength;
	}

	FHitResult HitResult;
	if (CheckAnchorCollision(PreviousAnchorPoint, AnchorPoint, HitResult))
	{
		AnchorPoint = HitResult.ImpactPoint;
		AttachedWireLength = FVector::Dist(StartLocation, AnchorPoint);
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

	const FVector WireStart = GetWireStartLocation();
	const FVector ToAnchor = AnchorPoint - WireStart;
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

	if (CurrentWireDistance > AttachedWireLength)
	{
		const FVector Velocity = CachedMovementComponent->Velocity;
		const float RadialSpeed = FVector::DotProduct(Velocity, CurrentWireDirection);

		if (RadialSpeed < 0.0f)
		{
			CachedMovementComponent->Velocity -= CurrentWireDirection * RadialSpeed;
		}

		const float DistanceError = CurrentWireDistance - AttachedWireLength;
		if (DistanceError > KINDA_SMALL_NUMBER)
		{
			const float CorrectionSpeed = DistanceError * 10.0f;
			CachedMovementComponent->Velocity += CurrentWireDirection * CorrectionSpeed;
		}
	}

	const float HeightDifference = AnchorPoint.Z - WireStart.Z;
	
	if (HeightDifference > 0 && HeightDifference < 200.0f) 
	{
		if (CachedMovementComponent->Velocity.Z > 0)
		{
			CachedMovementComponent->Velocity.Z *= 0.98f;
		}
	}
	else if (HeightDifference <= 0)
	{
		if (CachedMovementComponent->Velocity.Z > 0)
		{
			CachedMovementComponent->Velocity.Z *= 0.9f;
		}
	}

	const FVector PullForce = CalculatePullForce();
	const FVector InputForce = CalculateSwingInputForce();

	FVector TotalForce = PullForce + InputForce;

	const FVector Velocity = CachedMovementComponent->Velocity;
	const float SpeedSq = Velocity.SizeSquared();
	const float MaxSpeed = CachedMovementComponent->MaxWalkSpeed * MaxSwingSpeedMultiplier;

	if (SpeedSq > FMath::Square(MaxSpeed))
	{
		const FVector DragForce = -Velocity.GetSafeNormal() * (SpeedSq * SwingDragCoefficient);
		TotalForce += DragForce;
	}
	
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

FVector UTimeThiefWireComponent::CalculatePullForce() const
{
	if (!IsValid(CachedMovementComponent)) return FVector::ZeroVector;

	const FVector TangentVelocity = GetTangentVelocity(CachedMovementComponent->Velocity, CurrentWireDirection);
	const float TangentSpeedSq = TangentVelocity.SizeSquared();
	
	const float CentrifugalForceMagnitude = CachedMovementComponent->Mass * CentrifugalMassMultiplier * TangentSpeedSq / FMath::Max(CurrentWireDistance, MinWireLengthForPhysics);

	float TotalPullMagnitude = CentrifugalForceMagnitude + PullInForce;
	
	const float MaxPullForce = PullInForce * 2.0f;
	TotalPullMagnitude = FMath::Min(TotalPullMagnitude, MaxPullForce);

	return CurrentWireDirection * TotalPullMagnitude;
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

	const FVector TangentVelocity = GetTangentVelocity(CachedMovementComponent->Velocity, CurrentWireDirection);
	const float TangentSpeedSq = TangentVelocity.SizeSquared();

	FVector ForceDirection = FVector::ZeroVector;

	if (TangentSpeedSq > FMath::Square(100.0f))
	{
		const FVector SwingDir = TangentVelocity.GetSafeNormal();
		const float Projection = FVector::DotProduct(InputDirection, SwingDir);
		ForceDirection = SwingDir * Projection;
	}
	else
	{
		const float InputAlongWire = FVector::DotProduct(InputDirection, CurrentWireDirection);
		const FVector RadialInputComponent = CurrentWireDirection * InputAlongWire;
		const FVector RawTangentInput = InputDirection - RadialInputComponent;

		if (RawTangentInput.SizeSquared() > KINDA_SMALL_NUMBER)
		{
			ForceDirection = RawTangentInput.GetSafeNormal();
		}
	}

	return ForceDirection * SwingInputForce;
}

bool UTimeThiefWireComponent::ShouldRelease(float DeltaTime)
{
	return IsStuck(DeltaTime) || IsOnGroundTooLong(DeltaTime) || IsWireSnapping() || IsFacingAwayFromWire();
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

bool UTimeThiefWireComponent::IsFacingAwayFromWire() const
{
	if (!IsValid(CachedCharacter)) return false;

	const FVector LookDirection = GetAimDirection();
	const float DotProduct = FVector::DotProduct(LookDirection, CurrentWireDirection);

	return DotProduct < WireReleaseLookDotThreshold;
}

FVector UTimeThiefWireComponent::GetAimDirection() const
{
	if (!IsValid(CachedCharacter)) return FVector::ForwardVector;

	FVector Loc;
	FRotator Rot;
	CachedCharacter->GetActorEyesViewPoint(Loc, Rot);
	return Rot.Vector();
}

FVector UTimeThiefWireComponent::GetWireStartLocation() const
{
	if (!IsValid(CachedCharacter)) return GetOwner()->GetActorLocation();

	if (CachedCharacter->GetMesh()->DoesSocketExist(WireStartSocketName))
	{
		return CachedCharacter->GetMesh()->GetSocketLocation(WireStartSocketName);
	}

	return CachedCharacter->GetActorLocation();
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

bool UTimeThiefWireComponent::FindBestAnchorTarget(FVector& OutTargetLocation)
{
	UWorld* World = GetWorld();
	if (!World || !IsValid(CachedCharacter)) return false;

	FVector Start;
	FRotator Rot;
	CachedCharacter->GetActorEyesViewPoint(Start, Rot);

	const FVector AimDir = Rot.Vector();
	const FVector End = Start + AimDir * MaxWireLength;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetOwner());

	FHitResult DirectHit;
	bool bHit = World->LineTraceSingleByObjectType(
		DirectHit, Start, End,
		FCollisionObjectQueryParams(WireCollisionObjectTypes),
		QueryParams
	);

	if (bHit)
	{
		if (DirectHit.Distance >= MinTargetDistance)
		{
			if (bAllowFloorAttachment || DirectHit.ImpactNormal.Z < 0.7f)
			{
				OutTargetLocation = DirectHit.ImpactPoint;
				return true;
			}
		}
	}

	TArray<FHitResult> HitResults;
	bool bFoundAny = World->SweepMultiByObjectType(
		HitResults, Start, End, FQuat::Identity,
		FCollisionObjectQueryParams(WireCollisionObjectTypes),
		FCollisionShape::MakeSphere(AutoAimRadius),
		QueryParams
	);

	if (!bFoundAny) return false;

	float BestDistSq = FLT_MAX;
	FVector BestLocation = FVector::ZeroVector;
	bool bFoundCandidate = false;

	for (const FHitResult& Hit : HitResults)
	{
		float DistanceToPlayer = FVector::Dist(Start, Hit.ImpactPoint);
		if (DistanceToPlayer < MinTargetDistance) continue;

		if (!bAllowFloorAttachment && Hit.ImpactNormal.Z >= 0.7f) continue;

		const float DistSq = FMath::PointDistToLine(Hit.ImpactPoint, AimDir, Start);

		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			BestLocation = Hit.ImpactPoint;
			bFoundCandidate = true;
		}
	}

	if (bFoundCandidate)
	{
		OutTargetLocation = BestLocation;
		return true;
	}

	return false;
}

void UTimeThiefWireComponent::DrawWireLine() const
{
#if WITH_EDITOR
	if (CurrentState == EWireState::Idle || AnchorPoint.IsNearlyZero()) return;

	const UWorld* World = GetWorld();
	if (!World) return;

	DrawDebugLine(World, GetWireStartLocation(), AnchorPoint, DebugWireColor, false, 0.0f, 0, DebugWireThickness);
#endif
}
