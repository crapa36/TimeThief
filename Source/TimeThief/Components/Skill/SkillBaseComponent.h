// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SkillBaseComponent.generated.h"


class ATimeThiefCharacterBase;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class TIMETHIEF_API USkillBaseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	USkillBaseComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	
	virtual void OnRegister() override;
	
public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
	
	UFUNCTION()
	virtual void ActivateSkill(){}
	
	bool CanActivate() const;
	uint32 GetSkillId() const { return SkillId; }
	float GetRemainingCoolTime() const { return LeftCoolTime; }
	void ApplyServerCooldownMs(uint32 RemainingCooldownMs);

protected:
	UPROPERTY(EditDefaultsOnly, Category="Skill")
	uint32 SkillId = 0;

	bool bCanActivate = false;

	UPROPERTY(EditDefaultsOnly, Category="Skill")
	float CoolTime = 0;

	float LeftCoolTime = 0;

	UPROPERTY()
	TObjectPtr<ATimeThiefCharacterBase> OwnerCharacter;
};
