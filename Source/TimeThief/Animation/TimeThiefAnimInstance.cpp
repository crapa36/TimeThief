#include "TimeThiefAnimInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Character/TimeThiefCharacterBase.h"
#include "Components/Combat/TimeThiefPawnCombatComponent.h"
#include "Network/MovableNetworkEntityInterface.h"
#include "CharacterTrajectoryComponent.h"

UTimeThiefAnimInstance::UTimeThiefAnimInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bShouldMove = false;
	bIsFalling = false;
}

void UTimeThiefAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
}

void UTimeThiefAnimInstance::TriggerDoubleJump()
{
	bIsDoubleJumping = true;
}

void UTimeThiefAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!CharacterOwner)
	{
		CharacterOwner = Cast<ACharacter>(TryGetPawnOwner());
		if (CharacterOwner)
		{
			if (CharacterOwner->GetClass()->ImplementsInterface(UMovableNetworkEntityInterface::StaticClass()))
			{
				MovableNetworkInterface.SetObject(CharacterOwner);
				MovableNetworkInterface.SetInterface(Cast<IMovableNetworkEntityInterface>(CharacterOwner));
			}
			CharacterMovement = CharacterOwner->GetCharacterMovement();
			TrajectoryComponent = CharacterOwner->FindComponentByClass<UCharacterTrajectoryComponent>();
		}
	}

	if (!CharacterOwner || !CharacterMovement)
	{
		return;
	}
	
	if (MovableNetworkInterface.GetInterface())
	{
		if (!CharacterOwner->IsLocallyControlled())
		{
			const FVector2D NetworkVelocity2D = MovableNetworkInterface->GetNetworkVelocity2D();
			if (Velocity.SizeSquared2D() < 0.25f && !NetworkVelocity2D.IsNearlyZero(0.5f))
			{
				Velocity = FVector(NetworkVelocity2D.X, NetworkVelocity2D.Y, Velocity.Z);
			}
		}
		Velocity = MovableNetworkInterface->GetMoveStep();
	}
	else
	{
		Velocity = CharacterOwner->GetVelocity();
	}

	VerticalVelocity = Velocity.Z;
	GroundSpeed = Velocity.Size2D();

	const bool bIsRemoteCharacter = MovableNetworkInterface.GetInterface() && !CharacterOwner->IsLocallyControlled();
	const float MoveSpeedThreshold = bIsRemoteCharacter ? 0.5f : 1.0f;
	bHasVelocity = GroundSpeed > MoveSpeedThreshold;
	
	bIsFalling = CharacterMovement->IsFalling();

	if (!bIsFalling)
	{
		bIsDoubleJumping = false;
	}
	
	const bool bHasAcceleration = !CharacterMovement->GetCurrentAcceleration().IsNearlyZero();
	bShouldMove = bIsRemoteCharacter ? bHasVelocity : ((GroundSpeed > 0.01f) && bHasAcceleration);
	
	bIsMoving = bHasVelocity && !bIsFalling;

	if (ATimeThiefCharacterBase* CharacterBase = Cast<ATimeThiefCharacterBase>(CharacterOwner))
	{
		if (UTimeThiefPawnCombatComponent* CombatComp = CharacterBase->GetCombatComponent())
		{
			bIsAiming = CombatComp->IsAiming();
		}
	}
}