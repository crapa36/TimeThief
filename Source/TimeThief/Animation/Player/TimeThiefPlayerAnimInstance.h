#pragma once

#include "CoreMinimal.h"
#include "Animation/TimeThiefAnimInstance.h"
#include "GameplayTagContainer.h"
#include "TimeThiefPlayerAnimInstance.generated.h"

class ATimeThiefPlayerCharacter;
class UCharacterTrajectoryComponent;
class UTimeThiefWeaponComponentBase;
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

	UFUNCTION(BlueprintCallable, Category = "Combat|Spread")
	FVector2D ApplyFireSpread(float InMaxVerticalRecoil, float InMaxHorizontalRecoil, float InRecoilBuildupPerShot, float InSpreadBuildupPerShot);

	UFUNCTION(BlueprintCallable, Category = "Combat|Recoil")
	void SetRecoilRecoverySpeed(float InRecoilRecovery, float InSpreadRecovery);

	UFUNCTION(BlueprintPure, Category = "Combat|Spread")
	float GetCurrentSpreadAngle() const { return CurrentSpreadRatio * MaxSpreadAngle * AimSpreadMultiplier; }

	UFUNCTION(BlueprintPure, Category = "Combat|Spread")
	FVector2D GetAimOffset() const { return AimOffset; }

	UFUNCTION(BlueprintPure, Category = "Combat|Recoil")
	float GetRecoilBuildup() const { return RecoilBuildup; }

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Character Reference")
	TObjectPtr<ATimeThiefPlayerCharacter> PlayerCharacter;

	UPROPERTY(BlueprintReadOnly, Category = "Character Reference")
	TObjectPtr<UCharacterTrajectoryComponent> TrajectoryComponent;

	UPROPERTY(BlueprintReadOnly, Category = "Character Reference")
	TObjectPtr<UTimeThiefWireComponent> WireComponent;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UTimeThiefWeaponComponentBase> CurrentWeapon;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	FGameplayTag EquippedWeaponTag;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	FTransform LeftHandIKTransform;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	FName LeftHandIKSocketName = FName("LeftHandIK");

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bHasWeapon = false;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Aim")
	float AimPitch = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Aim")
	FVector AimDirection = FVector::ForwardVector;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Aim|Rig")
	FVector WorldAimLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Aim")
	float AimSpreadMultiplier = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Aim|Rig")
	bool bUseWeaponControlRigRotation = false;

	UPROPERTY(BlueprintReadOnly, Category = "Wire")
	bool bIsWireAttached = false;

	UPROPERTY(BlueprintReadOnly, Category = "Wire")
	bool bIsWireActive = false;

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

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Recoil")
	float RecoilBuildup = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Spread")
	float CurrentSpreadRatio = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Spread")
	FVector2D AimOffset = FVector2D::ZeroVector;

private:
	void UpdateWeaponData();
	void UpdateWireData();
	void UpdateWireHandIK(float DeltaSeconds);
	void UpdateRecoil(float DeltaSeconds);
	void UpdateSpreadAndRecoil(float DeltaSeconds);
	void UpdateAimDirection();
	void UpdateAimingState();

	float TargetRecoilAlpha = 0.0f;
	FVector2D TargetAimOffset = FVector2D::ZeroVector;

	float RecoilRecoverySpeed = 5.0f;
	float SpreadRecoverySpeed = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Recoil")
	float RecoilInterpSpeed = 15.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Spread")
	float AimOffsetInterpSpeed = 15.0f;
};