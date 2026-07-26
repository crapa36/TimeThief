#pragma once

#include "CoreMinimal.h"
#include "InteractionActorBase.h"
#include "ChestActor.generated.h"

class UTimelineComponent;
class UAnimSequenceBase;
class UNiagaraSystem;
class USkeletalMeshComponent;

UCLASS()
class TIMETHIEF_API AChestActor : public AInteractionActorBase
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AChestActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	virtual void Interact(const ATimeThiefPlayerCharacter* Player) override;

	UFUNCTION(BlueprintCallable, Category = "Chest")
	void OpenChest();

protected:
	void ResetToClosedPose();
	void UpdateInteractionWidgetLocation();
	void PlayRewardBurstFX();
	
	UFUNCTION()
	void HandleTimelineProgress(float Value);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chest", meta = (AllowPrivateAccess = "true"))
	float InteractionWidgetHeightOffset = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chest|VFX", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraSystem> RewardBurstFX = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chest|VFX", meta = (AllowPrivateAccess = "true"))
	FVector RewardBurstFXOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chest|VFX", meta = (AllowPrivateAccess = "true"))
	FVector RewardBurstFXScale = FVector{2.0f, 2.0f, 2.0f};

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Chest", meta = (AllowPrivateAccess = "true"))
	bool bIsOpened = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Chest|VFX", meta = (AllowPrivateAccess = "true"))
	bool bRewardBurstFXPlayed = false;
	
	UPROPERTY(EditAnywhere)
	TObjectPtr<UStaticMeshComponent> LidMesh;
	
	UPROPERTY(EditAnywhere)
	TObjectPtr<UTimelineComponent> ChestTimelineComponent;
	
	UPROPERTY(EditAnywhere, Category="Chest|Timeline")
	FRotator StartRotation = FRotator::ZeroRotator;
	
	UPROPERTY(EditAnywhere, Category="Chest|Timeline")
	FRotator TargetRotation = FRotator::ZeroRotator;
	
	UPROPERTY(EditAnywhere, Category="Chest|Timeline")
	TObjectPtr<UCurveFloat> OpenCurve = nullptr;
};
