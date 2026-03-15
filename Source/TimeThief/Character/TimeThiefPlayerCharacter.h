#pragma once

#include "CoreMinimal.h"
#include "Character/TimeThiefCharacterBase.h"
#include "TimeThiefPlayerCharacter.generated.h"

class AStoreActor;
class USpringArmComponent;
class UCameraComponent;
class UTimeThiefHeroComponent;
class UTimeThiefPlayerCombatComponent;
class UCharacterTrajectoryComponent;
class UTimeThiefPawnData;
class UTimeThiefWireComponent;

UCLASS()
class TIMETHIEF_API ATimeThiefPlayerCharacter : public ATimeThiefCharacterBase {
	GENERATED_BODY()

public:
	ATimeThiefPlayerCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void SetPawnData(const UTimeThiefPawnData* InPawnData);
	
	virtual UTimeThiefPawnCombatComponent* GetPawnCombatComponent() const override;

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Character")
	UTimeThiefHeroComponent* GetHeroComponent() const { return HeroComponent; }

	UFUNCTION(BlueprintCallable, Category = "TimeThief|MotionMatching")
	UCharacterTrajectoryComponent* GetCharacterTrajectoryComponent() const { return CharacterTrajectoryComponent; }

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Wire")
	UTimeThiefWireComponent* GetWireComponent() const { return WireComponent; }

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Camera")
	UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	void SetNearStore(const AStoreActor* InNearStore);
	const AStoreActor* GetNearStore() const;
protected:
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void BeginPlay() override;

	void OnPawnDataSet();

	UFUNCTION()
	void OnDeath(AActor* OwningActor);

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wire")
	TObjectPtr<UTimeThiefWireComponent> WireComponent;

	UPROPERTY(ReplicatedUsing = OnRep_PawnData)
	TObjectPtr<const UTimeThiefPawnData> PawnData;

	UPROPERTY()
	const AStoreActor* NearStore;
	
private:
	UFUNCTION()
	void OnRep_PawnData();
	
public:
	FORCEINLINE UTimeThiefPlayerCombatComponent* GetPlayerCombatComponent() const { return PlayerCombatComponent; }
};