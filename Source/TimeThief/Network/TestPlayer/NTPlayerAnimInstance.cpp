#include "NTPlayerAnimInstance.h"

#include "KismetAnimationLibrary.h"
#include "NTPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"

void UNTPlayerAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	
	OwnerPlayer = Cast<ANTPlayer>(TryGetPawnOwner());
}

void UNTPlayerAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	
	if (OwnerPlayer == nullptr)
	{
		OwnerPlayer = Cast<ANTPlayer>(TryGetPawnOwner());
	}
	
	if (OwnerPlayer == nullptr)
	{
		Speed = 0.f;
		IsAir = false;
		Velocity = FVector::ZeroVector;
		Direction = 0.f;
		
		return;
	}
	
	Velocity = OwnerPlayer->GetVelocity();
	Velocity.Z = 0.f;
	
	Speed = Velocity.Size();
	
	if (const UCharacterMovementComponent* MoveComp = OwnerPlayer->GetCharacterMovement())
	{
		IsAir = MoveComp->IsFalling();
	}
	
	Direction = UKismetAnimationLibrary::CalculateDirection(OwnerPlayer->GetVelocity(), OwnerPlayer->GetActorRotation());
}
