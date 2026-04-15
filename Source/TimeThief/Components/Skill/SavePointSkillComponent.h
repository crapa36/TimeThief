// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SkillBaseComponent.h"
#include "SavePointSkillComponent.generated.h"

class UNiagaraComponent;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class TIMETHIEF_API USavePointSkillComponent : public USkillBaseComponent
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category= "VFX")
	TObjectPtr<UNiagaraComponent> SavePointEffect;
	
	UPROPERTY(EditAnywhere, Category= "VFX")
	TObjectPtr<UAnimMontage> Montage;
	
public:
	// Sets default values for this component's properties
	USavePointSkillComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	
	virtual void OnRegister() override;
	
public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
	
	virtual void ActivateSkill() override;
	
protected:
	UFUNCTION()
	void OnFinished(UNiagaraComponent* FinishedComponent);
};
