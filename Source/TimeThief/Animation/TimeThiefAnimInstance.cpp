#include "Animation/TimeThiefAnimInstance.h"
#include "../Character/TimeThiefCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "../TimeThiefGameplayTags.h" // 태그 매니저 헤더 필요

void UTimeThiefAnimInstance::NativeInitializeAnimation() {
	Super::NativeInitializeAnimation();

	Character = Cast<ATimeThiefCharacter>(TryGetPawnOwner());
	if (Character) {
		MovementComponent = Character->GetCharacterMovement();
		// ASC 가져오기 (IAbilitySystemInterface를 통해)
		ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Character);
	}
}

void UTimeThiefAnimInstance::NativeUpdateAnimation(float DeltaTime) {
	Super::NativeUpdateAnimation(DeltaTime);

	if (Character == nullptr) {
		Character = Cast<ATimeThiefCharacter>(TryGetPawnOwner());
		if (Character) {
			MovementComponent = Character->GetCharacterMovement();
			ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Character);
		}
	}

	if (Character == nullptr || MovementComponent == nullptr) return;

	// 1. 이동 변수 업데이트
	Velocity = Character->GetVelocity();
	GroundSpeed = UKismetMathLibrary::VSizeXY(Velocity);
	bShouldMove = (GroundSpeed > 3.0f) && (MovementComponent->GetCurrentAcceleration() != FVector::ZeroVector);
	bIsFalling = MovementComponent->IsFalling();
	bIsCrouching = Character->bIsCrouched;

	// 2. GAS 상태 업데이트
	if (ASC) {
		FGameplayTagContainer OwnedTags;
		ASC->GetOwnedGameplayTags(OwnedTags);

		const FTimeThiefGameplayTags& Tags = FTimeThiefGameplayTags::Get();

		// 예: Aiming 태그가 있는지 확인 (태그는 나중에 등록해야 함)
		// bIsAiming = OwnedTags.HasTagExact(Tags.State_Combat_Aiming);

		// 무기 태그 확인 로직 (예시)
		// if (OwnedTags.HasTag(Tags.Weapon_Rifle)) CurrentWeaponTag = Tags.Weapon_Rifle;
	}
}