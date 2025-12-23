#include "Animation/Player/TimeThiefPlayerAnimInstance.h"
#include "Character/TimeThiefPlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "CharacterTrajectoryComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"
#include "TimeThiefGameplayTags.h" // [필수] 네이티브 태그 헤더 추가

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
		AbilitySystemComponent = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerCharacter);
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

	if (PlayerCharacter && !AbilitySystemComponent) {
		AbilitySystemComponent = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerCharacter);
	}

	if (PlayerCharacter) {
		Velocity = PlayerCharacter->GetVelocity();
		GroundSpeed = Velocity.Size2D();
		bIsMoving = GroundSpeed > 3.0f && !PlayerCharacter->GetCharacterMovement()->GetCurrentAcceleration().IsZero();

		
		if (AbilitySystemComponent) {
			
			bHasWeapon = AbilitySystemComponent->HasMatchingGameplayTag(FTimeThiefGameplayTags::Get().State_Combat_Rifle);
		}
		else {
			bHasWeapon = false;
		}
	}
}