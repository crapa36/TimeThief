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

	// Jump State 초기화
	JumpState = EJumpState::None;
	bIsJumping = false;
	TimeInAir = 0.0f;
	TimeSinceLanded = 0.0f;
	bWasFalling = false;
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

	UpdateJumpState(DeltaSeconds);
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

void UTimeThiefPlayerAnimInstance::UpdateJumpState(float DeltaSeconds) {
	// bIsFalling은 부모 클래스에서 이미 업데이트됨
	const bool bCurrentlyFalling = bIsFalling;

	// 점프 시작 감지 (지면에서 공중으로 전환)
	if (bCurrentlyFalling && !bWasFalling) {
		bIsJumping = true;
		TimeInAir = 0.0f;
		JumpState = EJumpState::JumpStart;
	}

	// 착지 감지 (공중에서 지면으로 전환)
	if (!bCurrentlyFalling && bWasFalling) {
		bIsJumping = false;
		TimeSinceLanded = 0.0f;
		JumpState = EJumpState::JumpEnd;
	}

	// 공중에 있을 때
	if (bCurrentlyFalling) {
		TimeInAir += DeltaSeconds;
		TimeSinceLanded = 0.0f;

		// JumpStart → JumpLoop 전환
		if (JumpState == EJumpState::JumpStart && TimeInAir >= JumpStartDuration) {
			JumpState = EJumpState::JumpLoop;
		}
	}
	// 지면에 있을 때
	else {
		TimeInAir = 0.0f;
		TimeSinceLanded += DeltaSeconds;

		// JumpEnd → None 전환
		if (JumpState == EJumpState::JumpEnd && TimeSinceLanded >= JumpEndDuration) {
			JumpState = EJumpState::None;
		}
	}

	bWasFalling = bCurrentlyFalling;
}