#include "TimeThiefAnimInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Character/TimeThiefCharacterBase.h"
#include "Components/Combat/TimeThiefPawnCombatComponent.h"
#include "Network/MovableNetworkEntityInterface.h"

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
		}
	}

	if (!CharacterOwner || !CharacterMovement)
	{
		return;
	}
	
	if (MovableNetworkInterface.GetInterface())
	{
		Velocity = MovableNetworkInterface->GetNetworkVelocity();
	}
	else
	{
		Velocity = CharacterOwner->GetVelocity();
	}

	VerticalVelocity = Velocity.Z;
	GroundSpeed = Velocity.Size2D();
	bHasVelocity = !FMath::IsNearlyZero(GroundSpeed);
	
	bIsFalling = CharacterMovement->IsFalling();
	
	const bool bHasAcceleration = !CharacterMovement->GetCurrentAcceleration().IsNearlyZero();
	bShouldMove = (GroundSpeed > 0.01f) && bHasAcceleration;
	
	bIsMoving = bHasVelocity && !bIsFalling;

	if (ATimeThiefCharacterBase* CharacterBase = Cast<ATimeThiefCharacterBase>(CharacterOwner))
	{
		if (UTimeThiefPawnCombatComponent* CombatComp = CharacterBase->GetCombatComponent())
		{
			bIsAiming = CombatComp->IsAiming();
		}
	}
}