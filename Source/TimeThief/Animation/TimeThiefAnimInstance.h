#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "TimeThiefAnimInstance.generated.h"

class ACharacter;
class UTimeThiefPawnCombatComponent;
class ATimeThiefWeaponBase;

UCLASS(Config = Game)
class TIMETHIEF_API UTimeThiefAnimInstance : public UAnimInstance {
	GENERATED_BODY()

public:
	UTimeThiefAnimInstance(const FObjectInitializer& ObjectInitializer);

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "References", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ACharacter> CharacterOwner;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "References", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTimeThiefPawnCombatComponent> CombatComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ATimeThiefWeaponBase> CurrentWeapon;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UAnimInstance> CurrentLinkedLayerClass;

private:
	void UpdateCombatComponent();
	void ProcessWeaponLayerUpdate();

	// Debug helper to prevent log spam
	float DebugLogTimer = 0.0f;
};