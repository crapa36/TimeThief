#include "Components/Wire/TimeThiefWirePhysics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Character.h"

void UTimeThiefWirePhysics::Initialize(UCharacterMovementComponent* InMovementComponent)
{
	CachedMovementComponent = InMovementComponent;
}

void UTimeThiefWirePhysics::ApplyWirePhysics(float DeltaTime, const FVector& AnchorPoint, const FVector& WireStartLocation, float WireLength, const FVector2D& Input)
{
	if (!IsValid(CachedMovementComponent)) return;

	const FVector ToAnchor = AnchorPoint - WireStartLocation;
	const float CurrentDistance = ToAnchor.Size();
	const FVector WireDirection = ToAnchor.GetSafeNormal();

	
	if (CurrentDistance > WireLength)
	{
		const FVector Velocity = CachedMovementComponent->Velocity;
		const float RadialSpeed = FVector::DotProduct(Velocity, WireDirection);

		if (RadialSpeed < 0.0f)
		{
			CachedMovementComponent->Velocity -= WireDirection * RadialSpeed;
		}

		const float DistanceError = CurrentDistance - WireLength;
		if (DistanceError > KINDA_SMALL_NUMBER)
		{
			const float CorrectionSpeed = DistanceError * PositionCorrectionSpeed;
			CachedMovementComponent->Velocity += WireDirection * CorrectionSpeed;
		}
	}

	
	const float HeightDifference = AnchorPoint.Z - WireStartLocation.Z;
	if (HeightDifference > 0 && HeightDifference < AnchorHeightThreshold)
	{
		if (CachedMovementComponent->Velocity.Z > 0)
		{
			CachedMovementComponent->Velocity.Z *= VerticalDampingLow;
		}
	}
	else if (HeightDifference <= 0)
	{
		if (CachedMovementComponent->Velocity.Z > 0)
		{
			CachedMovementComponent->Velocity.Z *= VerticalDampingHigh;
		}
	}

	
	const FVector PullForce = CalculatePullForce(WireDirection, CurrentDistance);
	const FVector InputForce = CalculateSwingInputForce(WireDirection, Input);
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

FVector UTimeThiefWirePhysics::CalculatePullForce(const FVector& WireDirection, float CurrentDistance) const
{
	if (!IsValid(CachedMovementComponent)) return FVector::ZeroVector;

	const FVector TangentVelocity = GetTangentVelocity(CachedMovementComponent->Velocity, WireDirection);
	const float TangentSpeedSq = TangentVelocity.SizeSquared();

	const float CentrifugalForceMagnitude = CachedMovementComponent->Mass * CentrifugalMassMultiplier * TangentSpeedSq / FMath::Max(CurrentDistance, MinWireLengthForPhysics);

	float TotalPullMagnitude = CentrifugalForceMagnitude + PullInForce;
	const float MaxPullForce = PullInForce * 2.0f;
	TotalPullMagnitude = FMath::Min(TotalPullMagnitude, MaxPullForce);

	return WireDirection * TotalPullMagnitude;
}

FVector UTimeThiefWirePhysics::CalculateSwingInputForce(const FVector& WireDirection, const FVector2D& Input) const
{
	if (!IsValid(CachedMovementComponent) || Input.IsNearlyZero()) return FVector::ZeroVector;

	const ACharacter* Character = Cast<ACharacter>(CachedMovementComponent->GetOwner());
	if (!Character) return FVector::ZeroVector;

	FVector ViewLoc;
	FRotator ViewRot;
	Character->GetActorEyesViewPoint(ViewLoc, ViewRot);

	const FVector ForwardDir = FRotationMatrix(ViewRot).GetUnitAxis(EAxis::X);
	const FVector RightDir = FRotationMatrix(ViewRot).GetUnitAxis(EAxis::Y);
	const FVector InputDirection = (ForwardDir * Input.Y + RightDir * Input.X).GetSafeNormal();

	if (InputDirection.IsNearlyZero()) return FVector::ZeroVector;

	const FVector TangentVelocity = GetTangentVelocity(CachedMovementComponent->Velocity, WireDirection);
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
		const float InputAlongWire = FVector::DotProduct(InputDirection, WireDirection);
		const FVector RadialInputComponent = WireDirection * InputAlongWire;
		const FVector RawTangentInput = InputDirection - RadialInputComponent;

		if (RawTangentInput.SizeSquared() > KINDA_SMALL_NUMBER)
		{
			ForceDirection = RawTangentInput.GetSafeNormal();
		}
	}

	return ForceDirection * SwingInputForce;
}

FVector UTimeThiefWirePhysics::GetTangentVelocity(const FVector& Velocity, const FVector& WireDirection) const
{
	const float VelocityAlongWire = FVector::DotProduct(Velocity, WireDirection);
	return Velocity - WireDirection * VelocityAlongWire;
}
