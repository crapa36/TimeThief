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

private:
	void UpdateWeaponData();
	void UpdateSway(float DeltaSeconds);

	FRotator LastRotation;
};
