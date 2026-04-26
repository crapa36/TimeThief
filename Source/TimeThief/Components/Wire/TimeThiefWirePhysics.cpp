#include "Components/Wire/TimeThiefWirePhysics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "Utils/TimeThiefAimStatics.h"

void UTimeThiefWirePhysics::Initialize(UCharacterMovementComponent* InMovementComponent)
{
	CachedMovementComponent = InMovementComponent;
}

void UTimeThiefWirePhysics::ApplyWirePhysics(float DeltaTime, const FVector& AnchorPoint, const FVector& WireStartLocation, float WireLength, const FVector2D& Input)
{
	if (!IsValid(CachedMovementComponent))
	{
		return;
	}

	const FVector ToAnchor = AnchorPoint - WireStartLocation;
	const float CurrentDistance = ToAnchor.Size();
	const FVector WireDirection = ToAnchor.GetSafeNormal();
	const FVector Velocity = CachedMovementComponent->Velocity;

	FVector TotalForce = FVector::ZeroVector;

	const float DistanceError = CurrentDistance - WireLength;
	if (DistanceError > 0.0f)
	{
		const float RadialVelocity = FVector::DotProduct(Velocity, WireDirection);
		const float SpringForce = DistanceError * SpringStiffness;
		const float DampingForce = -RadialVelocity * SpringDamping;
		TotalForce += WireDirection * (SpringForce + DampingForce);
	}

	const FVector TangentVelocity = Velocity - WireDirection * FVector::DotProduct(Velocity, WireDirection);
	const float TangentSpeed = TangentVelocity.Size();
	const float CentrifugalForce = CachedMovementComponent->Mass * TangentSpeed * TangentSpeed / FMath::Max(CurrentDistance, 1.0f);
	const float EffectivePullForce = PullForce - CentrifugalForce;

	if (EffectivePullForce > 0.0f)
	{
		TotalForce += WireDirection * EffectivePullForce;
	}

	if (!Input.IsNearlyZero())
	{
		if (const ACharacter* Character = Cast<ACharacter>(CachedMovementComponent->GetOwner()))
		{
			FVector ViewLoc = FVector::ZeroVector;
			FVector ViewDirection = FVector::ForwardVector;
			if (!UTimeThiefAimStatics::ResolveAimView(Character, ViewLoc, ViewDirection))
			{
				ViewLoc = Character->GetPawnViewLocation();
				ViewDirection = Character->GetActorForwardVector();
			}

			const FRotator ViewRot = UTimeThiefAimStatics::ResolveAimRotationFromDirection(ViewDirection);

			const FVector CameraRight = FRotationMatrix(ViewRot).GetUnitAxis(EAxis::Y);
			FVector WireRight = FVector::CrossProduct(WireDirection, FVector::UpVector);
			
			if (WireRight.IsNearlyZero())
			{
				WireRight = CameraRight;
			}
			else
			{
				WireRight.Normalize();
			}

			const FVector CameraForward = FRotationMatrix(ViewRot).GetUnitAxis(EAxis::X);
			const FVector WorldInputDirection = (CameraForward * Input.Y + CameraRight * Input.X).GetSafeNormal();
			const float SwingProjection = FVector::DotProduct(WorldInputDirection, WireRight);
			
			TotalForce += WireRight * SwingProjection * SwingInputForce;
		}
	}
	else if (WireResistance > 0.0f)
	{
		TotalForce -= Velocity * WireResistance;
	}

	const float HeightDifference = AnchorPoint.Z - WireStartLocation.Z;
	
	if (HeightDifference > 0.0f)
	{
		const float NormalizedHeight = HeightDifference / FMath::Max(WireLength, 1.0f);
		

		float SpeedRatio = Velocity.Size() / CachedMovementComponent->MaxWalkSpeed;
		float SpeedFactor = FMath::Square(FMath::Min(SpeedRatio, 1.0f));
		
		if (Input.IsNearlyZero())
		{
			SpeedFactor *= 0.5f;
		}

		const float HeightFactor = FMath::Square(FMath::Clamp(NormalizedHeight, 0.0f, 1.0f));

		const float VerticalDampingForce = HeightFactor * SpeedFactor * VerticalDamping;
		TotalForce.Z += VerticalDampingForce;
	}

	const float MaxSpeed = CachedMovementComponent->MaxWalkSpeed * MaxSwingSpeedMultiplier;
	const float SpeedSq = Velocity.SizeSquared();

	if (SpeedSq > FMath::Square(MaxSpeed))
	{
		const FVector VelocityDir = Velocity.GetSafeNormal();
		CachedMovementComponent->Velocity = VelocityDir * MaxSpeed;
		const float ForceDot = FVector::DotProduct(TotalForce, VelocityDir);
		if (ForceDot > 0.0f)
		{
			TotalForce -= VelocityDir * ForceDot;
		}
	}

	CachedMovementComponent->AddForce(TotalForce);
}
