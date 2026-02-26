#pragma once

#include "CoreMinimal.h"
#include "Animation/TimeThiefAnimInstance.h" 
#include "GameplayTagContainer.h"
#include "TimeThiefPlayerAnimInstance.generated.h"

class ATimeThiefPlayerCharacter;
class UCharacterTrajectoryComponent;
class ATimeThiefWeaponBase;
class UTimeThiefWireComponent;

UCLASS()
class TIMETHIEF_API UTimeThiefPlayerAnimInstance : public UTimeThiefAnimInstance {
	GENERATED_BODY()

public:
	UTimeThiefPlayerAnimInstance(const FObjectInitializer& ObjectInitializer);

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "Combat|Recoil")
	void TriggerRecoil(float Intensity = 1.0f);

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Character Reference")
	TObjectPtr<ATimeThiefPlayerCharacter> PlayerCharacter;

	UPROPERTY(BlueprintReadOnly, Category = "Character Reference")
	TObjectPtr<UCharacterTrajectoryComponent> TrajectoryComponent;

	UPROPERTY(BlueprintReadOnly, Category = "Character Reference")
	TObjectPtr<UTimeThiefWireComponent> WireComponent;

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

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Aim")
	float AimPitch = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Aim")
	float AimYaw = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Wire")
	bool bIsWireAttached;

	UPROPERTY(BlueprintReadOnly, Category = "Wire")
	bool bIsWireActive;

	UPROPERTY(BlueprintReadOnly, Category = "Wire")
	FVector AnchorDirection;

	UPROPERTY(BlueprintReadOnly, Category = "Wire")
	FVector SwingVelocity;

	UPROPERTY(BlueprintReadOnly, Category = "Wire")
	FTransform WireLeftHandIKTransform;

	UPROPERTY(BlueprintReadOnly, Category = "Wire")
	float WireLeftHandIKAlpha = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Wire")
	FVector WireAnchorDirectionWorld;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wire|Settings")
	float WireHandIKInterpSpeed = 12.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wire|Settings")
	float WireHandReachDistance = 30.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wire|Settings")
	FName WireHandBoneName = FName("hand_l");

	UPROPERTY(BlueprintReadWrite, Category = "Combat|Recoil")
	float RecoilAlpha = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Spread")
	float MaxSpreadAngle = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Spread")
	float SpreadIncreasePerShot = 0.15f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Spread")
	float SpreadRecoverySpeed = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Spread")
	float HorizontalRecoilRange = 0.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Spread")
	float VerticalRecoilAmount = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Spread")
	float AimRecoverySpeed = 3.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Spread")
	float CurrentSpreadRatio = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Spread")
	FVector2D AimOffset = FVector2D::ZeroVector;

public:
	UFUNCTION(BlueprintCallable, Category = "Combat|Spread")
	void ApplyFireSpread();

	UFUNCTION(BlueprintPure, Category = "Combat|Spread")
	float GetCurrentSpreadAngle() const { return CurrentSpreadRatio * MaxSpreadAngle; }

	UFUNCTION(BlueprintPure, Category = "Combat|Spread")
	FVector2D GetAimOffset() const { return AimOffset; }

private:
	void UpdateWeaponData();
	void UpdateWireData();
	void UpdateWireHandIK(float DeltaSeconds);
	void UpdateRecoil(float DeltaSeconds);
	void UpdateSpreadAndAim(float DeltaSeconds);
	void UpdateAimDirection();

	float TargetRecoilAlpha = 0.0f;
	FVector2D TargetAimOffset = FVector2D::ZeroVector;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Recoil")
	float RecoilInterpSpeed = 15.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Spread")
	float AimOffsetInterpSpeed = 15.0f;
};
