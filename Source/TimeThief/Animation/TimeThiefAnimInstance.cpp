#include "TimeThiefAnimInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

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
		CharacterMovement = CharacterOwner->GetCharacterMovement();
	}
}

void UTimeThiefAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!CharacterOwner || !CharacterMovement)
	{
		return;
	}

	Velocity = CharacterOwner->GetVelocity();
	VerticalVelocity = Velocity.Z;
	GroundSpeed = Velocity.Size2D();
	bHasVelocity = !FMath::IsNearlyZero(GroundSpeed);
	bIsFalling = CharacterMovement->IsFalling();
	bIsMoving = bHasVelocity && !bIsFalling;
}
