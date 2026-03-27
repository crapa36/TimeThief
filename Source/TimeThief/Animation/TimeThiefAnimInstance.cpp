#include "TimeThiefAnimInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Network/MovableNetworkEntityInterface.h"

UTimeThiefAnimInstance::UTimeThiefAnimInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UTimeThiefAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

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

void UTimeThiefAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// if (!CharacterOwner || !CharacterMovement)
	if (!CharacterOwner || !MovableNetworkInterface || !CharacterMovement)
	{
		return;
	}

	// Velocity = CharacterOwner->GetVelocity();
	Velocity = MovableNetworkInterface->GetNetworkVelocity();
	VerticalVelocity = Velocity.Z;
	GroundSpeed = Velocity.Size2D();
	bHasVelocity = !FMath::IsNearlyZero(GroundSpeed);
	bIsFalling = CharacterMovement->IsFalling();
	bIsMoving = bHasVelocity && !bIsFalling;
}
