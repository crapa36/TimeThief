#pragma once

#include "CoreMinimal.h"
#include "Character/TimeThiefCharacterBase.h"
#include "InputActionValue.h"
#include "TimeThiefPlayerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UTimeThiefInputConfig;
class UTimeThiefHeroCombatComponent;

UCLASS()
class TIMETHIEF_API ATimeThiefPlayerCharacter : public ATimeThiefCharacterBase {
	GENERATED_BODY()

public:
	ATimeThiefPlayerCharacter();

	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	virtual UTimeThiefPawnCombatComponent* GetPawnCombatComponent() const override;

protected:
	virtual void InitAbilityActorInfo() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	void Input_AbilityInputTagPressed(FGameplayTag InputTag);
	void Input_AbilityInputTagReleased(FGameplayTag InputTag);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera)
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera)
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Combat)
	TObjectPtr<UTimeThiefHeroCombatComponent> HeroCombatComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UTimeThiefInputConfig> InputConfig;

public:
	FORCEINLINE UTimeThiefHeroCombatComponent* GetHeroCombatComponent() const { return HeroCombatComponent; }
};