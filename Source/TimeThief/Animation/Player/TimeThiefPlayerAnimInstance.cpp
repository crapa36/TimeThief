#include "Animation/Player/TimeThiefPlayerAnimInstance.h"
#include "Character/TimeThiefPlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "CharacterTrajectoryComponent.h"

UTimeThiefPlayerAnimInstance::UTimeThiefPlayerAnimInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer) {
	bIsMoving = false;
	bHasWeapon = false;
	GroundSpeed = 0.0f;
}

void UTimeThiefPlayerAnimInstance::NativeInitializeAnimation() {
	Super::NativeInitializeAnimation();

	PlayerCharacter = Cast<ATimeThiefPlayerCharacter>(TryGetPawnOwner());
	if (PlayerCharacter) {
		TrajectoryComponent = PlayerCharacter->GetCharacterTrajectoryComponent();
	}
}

void UTimeThiefPlayerAnimInstance::NativeUpdateAnimation(float DeltaSeconds) {
	Super::NativeUpdateAnimation(DeltaSeconds);

	
	if (!PlayerCharacter) {
		PlayerCharacter = Cast<ATimeThiefPlayerCharacter>(TryGetPawnOwner());
	}

	
	if (PlayerCharacter && !TrajectoryComponent) {
		TrajectoryComponent = PlayerCharacter->GetCharacterTrajectoryComponent();
	}

	
	if (PlayerCharacter) {
		Velocity = PlayerCharacter->GetVelocity();
		GroundSpeed = Velocity.Size2D();
		bIsMoving = GroundSpeed > 3.0f && !PlayerCharacter->GetCharacterMovement()->GetCurrentAcceleration().IsZero();

		if (PlayerCharacter->GetHeroCombatComponent()) {
			bHasWeapon = false;
		}
	}
}