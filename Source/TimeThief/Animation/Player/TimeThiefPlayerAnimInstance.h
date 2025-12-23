#pragma once

#include "CoreMinimal.h"
#include "Animation/TimeThiefAnimInstance.h"
#include "GameplayTagContainer.h"
#include "TimeThiefPlayerAnimInstance.generated.h"

class ATimeThiefPlayerCharacter;
class UCharacterTrajectoryComponent;

UCLASS()
class TIMETHIEF_API UTimeThiefPlayerAnimInstance : public UTimeThiefAnimInstance {
	GENERATED_BODY()

public:
	UTimeThiefPlayerAnimInstance(const FObjectInitializer& ObjectInitializer);

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Character")
	TObjectPtr<ATimeThiefPlayerCharacter> PlayerCharacter;

	UPROPERTY(BlueprintReadOnly, Category = "Character")
	TObjectPtr<UCharacterTrajectoryComponent> TrajectoryComponent;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	FGameplayTag EquippedWeaponTag;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	FTransform LeftHandIKTransform;

	UPROPERTY(BlueprintReadOnly, Category = "Movement")
	FVector Velocity;

	UPROPERTY(BlueprintReadOnly, Category = "Movement")
	float GroundSpeed;

	UPROPERTY(BlueprintReadOnly, Category = "Movement")
	uint8 bIsMoving : 1;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	uint8 bHasWeapon : 1;
};