#include "TimeThiefAnimInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UTimeThiefAnimInstance::UTimeThiefAnimInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer) {
}

void UTimeThiefAnimInstance::NativeInitializeAnimation() {
	Super::NativeInitializeAnimation();

	CharacterOwner = Cast<ACharacter>(TryGetPawnOwner());
	if (CharacterOwner) {
		CharacterMovement = CharacterOwner->GetCharacterMovement();
	}
}

void UTimeThiefAnimInstance::NativeUpdateAnimation(float DeltaSeconds) {
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!CharacterOwner) {
		CharacterOwner = Cast<ACharacter>(TryGetPawnOwner());
		if (CharacterOwner) {
			CharacterMovement = CharacterOwner->GetCharacterMovement();
		}
		return;
	}

	UpdateCharacterState();
	UpdateLocomotionData();
}

void UTimeThiefAnimInstance::UpdateCharacterState() {
	if (!CharacterMovement) {
		return;
	}

	bIsFalling = CharacterMovement->IsFalling();
	bIsJumping = bIsFalling && (Velocity.Z > 0.0f);
}

void UTimeThiefAnimInstance::UpdateLocomotionData() {
	if (!CharacterOwner) {
		return;
	}

	Velocity = CharacterOwner->GetVelocity();
	VerticalVelocity = Velocity.Z;
	
	const FVector LateralVelocity = FVector(Velocity.X, Velocity.Y, 0.0f);
	GroundSpeed = LateralVelocity.Size();
	
	bHasVelocity = !FMath::IsNearlyZero(GroundSpeed);
	bIsMoving = bHasVelocity && !bIsFalling;
}

