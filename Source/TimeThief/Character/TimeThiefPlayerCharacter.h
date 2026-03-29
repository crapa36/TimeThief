#pragma once

#include "CoreMinimal.h"
#include "TimeThiefNetworkCharacterBase.h"
#include "Character/TimeThiefCharacterBase.h"
#include "TimeThiefPlayerCharacter.generated.h"

class AInteractionActorBase;
class UInventorySystemComponent;
class AItemBase;
class AStoreActor;
class USpringArmComponent;
class UCameraComponent;
class UTimeThiefHeroComponent;
class UTimeThiefPlayerCombatComponent;
class UCharacterTrajectoryComponent;
class UTimeThiefPawnData;
class UTimeThiefWireComponent;

DECLARE_MULTICAST_DELEGATE(FOnVicinityItemUpdatedEvent);

UCLASS()
class TIMETHIEF_API ATimeThiefPlayerCharacter : public ATimeThiefNetworkCharacterBase {
	GENERATED_BODY()

public:
	ATimeThiefPlayerCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
	void OnInteract();
	
	void SetPawnData(const UTimeThiefPawnData* InPawnData);
	
	virtual UTimeThiefPawnCombatComponent* GetCombatComponent() const override;

	virtual USkeletalMeshComponent* GetWeaponAttachMesh() const override;
	virtual USkeletalMeshComponent* GetMontagePlaybackMesh() const override;

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Character")
	UTimeThiefHeroComponent* GetHeroComponent() const { return HeroComponent; }

	UFUNCTION(BlueprintCallable, Category = "TimeThief|MotionMatching")
	UCharacterTrajectoryComponent* GetCharacterTrajectoryComponent() const { return CharacterTrajectoryComponent; }

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Wire")
	UTimeThiefWireComponent* GetWireComponent() const { return WireComponent; }
	
	UInventorySystemComponent* GetInventoryComponent() const { return InventoryComponent; }
	
	UFUNCTION(BlueprintCallable, Category = "TimeThief|Camera")
	UCameraComponent* GetFollowCamera() const { return FollowCamera; }
	
	void AddVicinityItem(AItemBase* Item);
	void RemoveVicinityItem(AItemBase* Item);
	const TArray<TObjectPtr<AItemBase>>& GetVicinityItems() const { return VicinityItem; }
	
	FOnVicinityItemUpdatedEvent OnVicinityItemUpdatedEvent;
	
protected:
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;
	virtual void BeginPlay() override;
	
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_Controller() override;
	virtual void OnRep_PlayerState() override;

	void OnPawnDataSet();

	UFUNCTION()
	void OnDeath(AActor* OwningActor);

	void CheckInteractableObject();
	
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
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UInventorySystemComponent> InventoryComponent;
	
	UPROPERTY(ReplicatedUsing = OnRep_PawnData)
	TObjectPtr<const UTimeThiefPawnData> PawnData;
	
	UPROPERTY()
	TArray<TObjectPtr<AItemBase>> VicinityItem;
	
	UPROPERTY()
	TWeakObjectPtr<AInteractionActorBase> CurrentLookingActor;
	
	UPROPERTY(EditDefaultsOnly)
	float LookingDistance = 50.f;
	
	FTimerHandle InteractCheckTimerHandle;
	
private:
	UFUNCTION()
	void OnRep_PawnData();
	
public:
	FORCEINLINE UTimeThiefPlayerCombatComponent* GetPlayerCombatComponent() const { return PlayerCombatComponent; }
};