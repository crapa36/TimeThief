#include "Animation/Player/TimeThiefPlayerAnimInstance.h"
#include "Character/TimeThiefPlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "CharacterTrajectoryComponent.h"
#include "Components/Combat/TimeThiefPawnCombatComponent.h"
#include "Weapon/TimeThiefWeaponBase.h"

UTimeThiefPlayerAnimInstance::UTimeThiefPlayerAnimInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer) {
	bIsMoving = false;
	bHasWeapon = false;
	GroundSpeed = 0.0f;
	LeftHandIKTransform = FTransform::Identity;
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

	if (PlayerCharacter) {
		if (!TrajectoryComponent) {
			TrajectoryComponent = PlayerCharacter->GetCharacterTrajectoryComponent();
		}

		Velocity = PlayerCharacter->GetVelocity();
		GroundSpeed = Velocity.Size2D();
		bIsMoving = GroundSpeed > 3.0f && !PlayerCharacter->GetCharacterMovement()->GetCurrentAcceleration().IsZero();

		if (UTimeThiefPawnCombatComponent* CombatComp = PlayerCharacter->GetPawnCombatComponent()) {
			ATimeThiefWeaponBase* EquippedWeapon = CombatComp->GetCharacterCurrentEquippedWeapon();

			if (EquippedWeapon) {
				bHasWeapon = true;
				EquippedWeaponTag = EquippedWeapon->GetWeaponTag();

				if (USkeletalMeshComponent* WeaponMesh = EquippedWeapon->GetWeaponMesh()) {
					if (WeaponMesh->DoesSocketExist(FName("LeftHandSocket"))) {
						FTransform SocketTransform = WeaponMesh->GetSocketTransform(FName("LeftHandSocket"), RTS_World);
						FTransform MeshTransform = PlayerCharacter->GetMesh()->GetComponentTransform();
						LeftHandIKTransform = SocketTransform.GetRelativeTransform(MeshTransform);
					}
				}
			}
			else {
				bHasWeapon = false;
				EquippedWeaponTag = FGameplayTag();
			}
		}
	}
}