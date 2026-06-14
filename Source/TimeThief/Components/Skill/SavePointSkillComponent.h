// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ItemCommons.h"
#include "SkillBaseComponent.h"
#include "Character/TimeThiefPlayerState.h"
#include "Interface/LifeObserver.h"
#include "SavePointSkillComponent.generated.h"

class UNiagaraComponent;
class UInventoryObject;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class TIMETHIEF_API USavePointSkillComponent : public USkillBaseComponent, public ILifeObserver
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category= "VFX")
	TObjectPtr<UNiagaraComponent> SavePointEffect;
	
	UPROPERTY(EditAnywhere, Category= "VFX")
	TObjectPtr<UAnimMontage> Montage;
public:
	virtual void OnBeginRespawn() override;
	
public:
	// Sets default values for this component's properties
	USavePointSkillComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	
	virtual void OnRegister() override;
	
public:
	virtual void ActivateSkill() override;

protected:
	UFUNCTION()
	void OnFinished(UNiagaraComponent* FinishedComponent);
	
	void Save();
	
	FVector SavedLocation;
	FStatus SavedStatus;
	TArray<TPair<EItemID,int>> SavedInventory;
};
