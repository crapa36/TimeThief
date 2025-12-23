#pragma once

#include "CoreMinimal.h"
#include "Animation/TimeThiefAnimInstance.h"
#include "TimeThiefPlayerAnimInstance.generated.h"

class UCharacterTrajectoryComponent;
class ATimeThiefPlayerCharacter;
class UAbilitySystemComponent;

UCLASS()
class TIMETHIEF_API UTimeThiefPlayerAnimInstance : public UTimeThiefAnimInstance {
	GENERATED_BODY()

public:
	UTimeThiefPlayerAnimInstance(const FObjectInitializer& ObjectInitializer);

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|Ref")
	TObjectPtr<ATimeThiefPlayerCharacter> PlayerCharacter;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|MotionMatching")
	TObjectPtr<UCharacterTrajectoryComponent> TrajectoryComponent;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "TimeThief|MotionMatching")
	bool bIsMoving;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "TimeThief|MotionMatching")
	bool bHasWeapon; 
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "TimeThief|MotionMatching")
	FVector Velocity;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "TimeThief|MotionMatching")
	float GroundSpeed;
};