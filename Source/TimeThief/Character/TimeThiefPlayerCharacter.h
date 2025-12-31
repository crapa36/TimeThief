#pragma once

#include "CoreMinimal.h"
#include "Character/TimeThiefCharacterBase.h"
#include "TimeThiefPlayerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UTimeThiefHeroComponent;
class UTimeThiefPlayerCombatComponent;
class UCharacterTrajectoryComponent;
class UTimeThiefPawnData;

UCLASS()
class TIMETHIEF_API ATimeThiefPlayerCharacter : public ATimeThiefCharacterBase {
	GENERATED_BODY()

public:
	ATimeThiefPlayerCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void SetPawnData(const UTimeThiefPawnData* InPawnData);

	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void OnRep_PlayerState() override;

	virtual UTimeThiefPawnCombatComponent* GetPawnCombatComponent() const override;

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Character")
	UTimeThiefHeroComponent* GetHeroComponent() const { return HeroComponent; }

	UFUNCTION(BlueprintCallable, Category = "TimeThief|MotionMatching")
	UCharacterTrajectoryComponent* GetCharacterTrajectoryComponent() const { return CharacterTrajectoryComponent; }

protected:
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void BeginPlay() override;

	void OnPawnDataSet();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera)
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera)
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TimeThief|Character")
	TObjectPtr<UTimeThiefHeroComponent> HeroComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Combat)
	TObjectPtr<UTimeThiefPlayerCombatComponent> PlayerCombatComponent;


	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MotionMatching")
	TObjectPtr<UCharacterTrajectoryComponent> CharacterTrajectoryComponent;

	UPROPERTY(ReplicatedUsing = OnRep_PawnData)
	TObjectPtr<const UTimeThiefPawnData> PawnData;

private:
	UFUNCTION()
	void OnRep_PawnData();

public:
	FORCEINLINE UTimeThiefPlayerCombatComponent* GetPlayerCombatComponent() const { return PlayerCombatComponent; }
};