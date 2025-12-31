#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "TimeThiefAnimInstance.generated.h"

class ACharacter;
class UCharacterMovementComponent;

UCLASS(Config = Game)
class TIMETHIEF_API UTimeThiefAnimInstance : public UAnimInstance {
	GENERATED_BODY()

public:
	UTimeThiefAnimInstance(const FObjectInitializer& ObjectInitializer);

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "References")
	TObjectPtr<ACharacter> CharacterOwner;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "References")
	TObjectPtr<UCharacterMovementComponent> CharacterMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Locomotion")
	FVector Velocity;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Locomotion")
	float GroundSpeed;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Locomotion")
	bool bHasVelocity;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Locomotion")
	bool bIsMoving;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Jump")
	bool bIsFalling;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Jump")
	bool bIsJumping;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Jump")
	float VerticalVelocity;

private:
	void UpdateCharacterState();
	void UpdateLocomotionData();
};