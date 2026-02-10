#pragma once

#include "CoreMinimal.h"
#include "Animation/TimeThiefAnimInstance.h"
#include "GameplayTagContainer.h"
#include "TimeThiefFirstPersonAnimInstance.generated.h"

class ATimeThiefPlayerCharacter;
class ATimeThiefWeaponBase;

UCLASS()
class TIMETHIEF_API UTimeThiefFirstPersonAnimInstance : public UTimeThiefAnimInstance
{
	GENERATED_BODY()

public:
	UTimeThiefFirstPersonAnimInstance(const FObjectInitializer& ObjectInitializer);

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Character Reference")
	TObjectPtr<ATimeThiefPlayerCharacter> PlayerCharacter;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	TObjectPtr<ATimeThiefWeaponBase> CurrentWeapon;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	FGameplayTag EquippedWeaponTag;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bHasWeapon;

	UPROPERTY(BlueprintReadOnly, Category = "FirstPerson|Sway")
	FRotator SwayRotation;

	UPROPERTY(BlueprintReadOnly, Category = "FirstPerson|Sway")
	FVector SwayLocation;

	UPROPERTY(EditDefaultsOnly, Category = "FirstPerson|Sway")
	float SwaySpeed = 10.0f;

	UPROPERTY(EditDefaultsOnly, Category = "FirstPerson|Sway")
	float MaxSwayDegree = 5.0f;

	UPROPERTY(EditDefaultsOnly, Category = "FirstPerson|Sway")
	float MaxSwayDistance = 2.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Procedural")
	float ProceduralSpeed;

	UPROPERTY(BlueprintReadOnly, Category = "Procedural")
	FVector ProceduralVelocity;

	UPROPERTY(BlueprintReadOnly, Category = "Procedural")
	FRotator DeltaRotation;

	UPROPERTY(BlueprintReadOnly, Category = "Procedural")
	float AccumulatedTime;

	UPROPERTY(EditDefaultsOnly, Category = "Procedural|Breathing")
	float BreathingSpeed = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Procedural|Breathing")
	float BreathingAmplitude = 0.8f;

	UPROPERTY(EditDefaultsOnly, Category = "Procedural|Bobbing")
	float IdleBobAmplitude = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Procedural|Bobbing")
	float WalkBobAmplitude = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Procedural|Bobbing")
	float RunBobAmplitude = 5.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Procedural|Bobbing")
	float WalkSpeedThreshold = 200.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Procedural|Bobbing")
	float RunSpeedThreshold = 500.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Procedural|Bobbing")
	float CurrentBobAmplitude;

	UPROPERTY(EditDefaultsOnly, Category = "Procedural|Bobbing")
	float BobAmplitudeInterpSpeed = 8.0f;

private:
	void UpdateWeaponData();
	void UpdateSway(float DeltaSeconds);
	void UpdateProceduralData(float DeltaSeconds);

	FRotator LastRotation;
};
