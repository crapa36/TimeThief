#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GameplayTagContainer.h"
#include "Character/TimeThiefCharacterBase.h"
#include "TimeThiefAnimInstance.generated.h"

class ATimeThiefCharacterBase;
class UAbilitySystemComponent;
class UCharacterMovementComponent;

/**
 * GAS 태그와 캐릭터 상태를 애니메이션 변수로 변환하는 클래스
 */
UCLASS()
class TIMETHIEF_API UTimeThiefAnimInstance : public UAnimInstance {
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaTime) override;

protected:
	// --- 참조 변수 ---
	UPROPERTY(BlueprintReadOnly, Category = "Character")
	TObjectPtr<ATimeThiefCharacterBase> Character;

	UPROPERTY(BlueprintReadOnly, Category = "Character")
	TObjectPtr<UCharacterMovementComponent> MovementComponent;

	UPROPERTY(BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UAbilitySystemComponent> ASC;

	// --- 이동 관련 변수 (Locomotion) ---
	UPROPERTY(BlueprintReadOnly, Category = "Anim|Movement")
	FVector Velocity;

	UPROPERTY(BlueprintReadOnly, Category = "Anim|Movement")
	float GroundSpeed;

	UPROPERTY(BlueprintReadOnly, Category = "Anim|Movement")
	bool bShouldMove;

	UPROPERTY(BlueprintReadOnly, Category = "Anim|Movement")
	bool bIsFalling;

	UPROPERTY(BlueprintReadOnly, Category = "Anim|Movement")
	bool bIsCrouching;

	// --- 전투 관련 변수 (Combat) ---
	// GAS 태그를 기반으로 업데이트됨
	UPROPERTY(BlueprintReadOnly, Category = "Anim|Combat")
	bool bIsAiming;

	UPROPERTY(BlueprintReadOnly, Category = "Anim|Combat")
	bool bIsFiring;

	// 현재 장착된 무기 타입 (태그로 구분)
	UPROPERTY(BlueprintReadOnly, Category = "Anim|Combat")
	FGameplayTag CurrentWeaponTag;
};