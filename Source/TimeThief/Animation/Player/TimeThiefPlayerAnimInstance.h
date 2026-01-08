#pragma once

#include "CoreMinimal.h"
#include "Animation/TimeThiefAnimInstance.h" 
#include "GameplayTagContainer.h"
#include "TimeThiefPlayerAnimInstance.generated.h"

class ATimeThiefPlayerCharacter;
class UCharacterTrajectoryComponent;
class ATimeThiefWeaponBase;

UCLASS()
class TIMETHIEF_API UTimeThiefPlayerAnimInstance : public UTimeThiefAnimInstance {
	GENERATED_BODY()

public:
	UTimeThiefPlayerAnimInstance(const FObjectInitializer& ObjectInitializer);

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Character Reference")
	TObjectPtr<ATimeThiefPlayerCharacter> PlayerCharacter;

	UPROPERTY(BlueprintReadOnly, Category = "Character Reference")
	TObjectPtr<UCharacterTrajectoryComponent> TrajectoryComponent;

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
};