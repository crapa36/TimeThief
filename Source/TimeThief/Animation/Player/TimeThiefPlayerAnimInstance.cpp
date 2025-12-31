#include "Animation/Player/TimeThiefPlayerAnimInstance.h"
#include "Character/TimeThiefPlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "CharacterTrajectoryComponent.h"
#include "Components/Combat/TimeThiefPawnCombatComponent.h"
#include "Weapon/TimeThiefWeaponBase.h"

UTimeThiefPlayerAnimInstance::UTimeThiefPlayerAnimInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer) {
	bHasWeapon = false;
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
		if (!PlayerCharacter) {
			return;
		}
	}

	if (!TrajectoryComponent) {
		TrajectoryComponent = PlayerCharacter->GetCharacterTrajectoryComponent();
	}

	UpdateWeaponData();
}

void UTimeThiefPlayerAnimInstance::UpdateWeaponData() {
	if (!PlayerCharacter) {
		CurrentWeapon = nullptr;
		bHasWeapon = false;
		EquippedWeaponTag = FGameplayTag();
		return;
	}

	UTimeThiefPawnCombatComponent* CombatComp = PlayerCharacter->GetPawnCombatComponent();
	if (!CombatComp) {
		CurrentWeapon = nullptr;
		bHasWeapon = false;
		EquippedWeaponTag = FGameplayTag();
		return;
	}

	CurrentWeapon = CombatComp->GetCharacterCurrentEquippedWeapon();
	if (!CurrentWeapon) {
		bHasWeapon = false;
		EquippedWeaponTag = FGameplayTag();
		return;
	}

	bHasWeapon = true;
	EquippedWeaponTag = CurrentWeapon->GetWeaponTag();

	USkeletalMeshComponent* WeaponMesh = CurrentWeapon->GetWeaponMesh();
	if (WeaponMesh && WeaponMesh->DoesSocketExist(LeftHandIKSocketName)) {
		FTransform SocketTransform = WeaponMesh->GetSocketTransform(LeftHandIKSocketName, RTS_World);
		FTransform MeshTransform = PlayerCharacter->GetMesh()->GetComponentTransform();
		LeftHandIKTransform = SocketTransform.GetRelativeTransform(MeshTransform);
	}
}