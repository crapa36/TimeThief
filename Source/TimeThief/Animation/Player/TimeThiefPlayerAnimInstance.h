#pragma once

#include "CoreMinimal.h"
#include "Animation/TimeThiefAnimInstance.h"
#include "GameplayTagContainer.h"
#include "TimeThiefPlayerAnimInstance.generated.h"

class ATimeThiefPlayerCharacter;
class UCharacterTrajectoryComponent;
class ATimeThiefWeaponBase;

UENUM(BlueprintType)
enum class EJumpState : uint8 {
	None		UMETA(DisplayName = "None"),
	JumpStart	UMETA(DisplayName = "Jump Start"),
	JumpLoop	UMETA(DisplayName = "Jump Loop"),
	JumpEnd		UMETA(DisplayName = "Jump End")
};

UCLASS()
class TIMETHIEF_API UTimeThiefPlayerAnimInstance : public UTimeThiefAnimInstance {
	GENERATED_BODY()

public:
	UTimeThiefPlayerAnimInstance(const FObjectInitializer& ObjectInitializer);

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	// Character References
	UPROPERTY(BlueprintReadOnly, Category = "Character")
	TObjectPtr<ATimeThiefPlayerCharacter> PlayerCharacter;

	UPROPERTY(BlueprintReadOnly, Category = "Character")
	TObjectPtr<UCharacterTrajectoryComponent> TrajectoryComponent;

	// Jump State for Chooser + State Machine
	UPROPERTY(BlueprintReadOnly, Category = "Jump")
	EJumpState JumpState;

	UPROPERTY(BlueprintReadOnly, Category = "Jump")
	bool bIsJumping;

	UPROPERTY(BlueprintReadOnly, Category = "Jump")
	float TimeInAir;

	UPROPERTY(BlueprintReadOnly, Category = "Jump")
	float TimeSinceLanded;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jump")
	float JumpStartDuration = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jump")
	float JumpEndDuration = 0.3f;

	// Combat
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	TObjectPtr<ATimeThiefWeaponBase> CurrentWeapon;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	FGameplayTag EquippedWeaponTag;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	FTransform LeftHandIKTransform;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	FName LeftHandIKSocketName = FName("LeftHandSocket");

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bHasWeapon;

private:
	void UpdateWeaponData();
	void UpdateJumpState(float DeltaSeconds);

	bool bWasFalling;
};