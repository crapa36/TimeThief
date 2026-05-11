#pragma once

#include "CoreMinimal.h"
#include "TimeThiefNetworkCharacterBase.h"
#include "GameplayTagContainer.h"
#include "DataAssets/UpgradeData.h"
#include "Components/Wire/TimeThiefWireTypes.h"
#include "TimeThiefPlayerCharacter.generated.h"

class AInteractionActorBase;
class UInventorySystemComponent;
class AItemBase;
class USpringArmComponent;
class UCameraComponent;
class UTimeThiefPlayerCombatComponent;
class UTimeThiefThrowableComponent;
class UTimeThiefHeroComponent;
class UCharacterTrajectoryComponent;
class UTimeThiefPawnData;
class UTimeThiefWireComponent;
class UNetworkWireComponent;
class UNiagaraSystem;

DECLARE_MULTICAST_DELEGATE(FOnVicinityItemUpdatedEvent);

UCLASS()
class TIMETHIEF_API ATimeThiefPlayerCharacter : public ATimeThiefNetworkCharacterBase {
	GENERATED_BODY()

public:
	ATimeThiefPlayerCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
	bool PurchaseItem(const FStoreOrder& Order);
	
	void OnInteract();
	
	void SetPawnData(const UTimeThiefPawnData* InPawnData);

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Character")
	const UTimeThiefPawnData* GetPawnData() const { return PawnData; }
	
	virtual UTimeThiefPawnCombatComponent* GetCombatComponent() const override;
	virtual void OnJumped_Implementation() override;
	virtual void Landed(const FHitResult& Hit) override;

	virtual USkeletalMeshComponent* GetWeaponAttachMesh() const override;
	virtual USkeletalMeshComponent* GetMontagePlaybackMesh() const override;

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Character")
	UTimeThiefHeroComponent* GetHeroComponent() const { return HeroComponent; }

	UFUNCTION(BlueprintCallable, Category = "TimeThief|MotionMatching")
	UCharacterTrajectoryComponent* GetCharacterTrajectoryComponent() const { return CharacterTrajectoryComponent; }

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Wire")
	UTimeThiefWireComponent* GetWireComponent() const { return WireComponent; }
	
	UInventorySystemComponent* GetInventoryComponent() const { return InventoryComponent; }

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Throwable")
	UTimeThiefThrowableComponent* GetThrowableComponent() const { return ThrowableComponent; }
	
	UFUNCTION(BlueprintCallable, Category = "TimeThief|Camera")
	UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	UFUNCTION(BlueprintPure, Category = "TimeThief|Movement")
	float GetBaseMoveSpeed() const { return BaseMoveSpeed; }

	UFUNCTION(BlueprintPure, Category = "TimeThief|Movement")
	float GetBaseJumpVelocity() const { return BaseJumpVelocity; }
	
	void AddVicinityItem(AItemBase* Item);
	void RemoveVicinityItem(AItemBase* Item);
	const TArray<TObjectPtr<AItemBase>>& GetVicinityItems() const { return VicinityItem; }
	
	FOnVicinityItemUpdatedEvent OnVicinityItemUpdatedEvent;
	

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;
	
	virtual void NotifyControllerChanged() override;
	
	virtual void PossessedBy(AController* NewController) override;
	virtual void Tick(float DeltaSeconds) override;
		
	virtual void OnDeath() override;
	virtual void OnBeginRespawn() override;
	virtual void OnEndRespawn() override;
	
protected:
	virtual void BeginPlay() override;
	virtual void ApplyPerspective() override;
	virtual void SendJumpEventToServer();
	virtual void SendJumpLandEventToServer();

	void OnPawnDataSet();
	
	UFUNCTION()
	void OnWireStateChanged(EWireState OldState, EWireState NewState);
	
	void CheckInteractableObject();
	
protected:
	UPROPERTY(EditDefaultsOnly, Category = "TimeThief|Character")
	TObjectPtr<const UTimeThiefPawnData> DefaultPawnData;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wire")
	TObjectPtr<UNetworkWireComponent> NetworkWireComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UInventorySystemComponent> InventoryComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Throwable")
	TObjectPtr<UTimeThiefThrowableComponent> ThrowableComponent;
	
	UPROPERTY()
	TObjectPtr<const UTimeThiefPawnData> PawnData;
	
	UPROPERTY()
	TArray<TObjectPtr<AItemBase>> VicinityItem;
	
	UPROPERTY()
	TWeakObjectPtr<AInteractionActorBase> CurrentLookingActor;
	
	UPROPERTY(EditDefaultsOnly)
	float LookingDistance = 50.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Camera")
	float DefaultCameraLagSpeed = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Camera")
	float WireCameraLagSpeed = 8.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Movement")
	float BaseMoveSpeed = 600.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Movement")
	float BaseJumpVelocity = 600.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Store|Upgrade")
	TArray<float> MoveSpeedBonusPerLevel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Store|Upgrade")
	TArray<float> JumpVelocityBonusPerLevel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Store|Upgrade")
	TMap<FGameplayTag, FUpgradeFloatLevels> DamageBonusByWeaponAndLevel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Store|Upgrade")
	TMap<FGameplayTag, FUpgradeIntLevels> CapacityBonusByWeaponAndLevel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Store|Upgrade")
	TMap<FGameplayTag, FUpgradeFloatLevels> RecoilReductionByWeaponAndLevel;
	
	FTimerHandle InteractCheckTimerHandle;
	float CachedCameraLagSpeed = 0.0f;
	
public:
	FORCEINLINE UTimeThiefPlayerCombatComponent* GetPlayerCombatComponent() const { return PlayerCombatComponent; }
};
