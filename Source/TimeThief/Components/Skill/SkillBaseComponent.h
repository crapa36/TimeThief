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
	virtual void OnRegister() override;
	
public:
	UFUNCTION()
	virtual void ActivateSkill(){}
	
	bool CanActivate() const;
	uint32 GetSkillId() const { return SkillId; }
	float GetRemainingCoolTime() const;
	float GetCooldownPercent() const;
	void ApplyServerCooldownMs(uint32 RemainingCooldownMs);

protected:
	void StartCooldown(float DurationSeconds);

	UPROPERTY(EditDefaultsOnly, Category="Skill")
	uint32 SkillId = 0;

	bool bCanActivate = false;

	float CooldownEndTimeSeconds = 0.0f;
	float CooldownDurationSeconds = 0.0f;

	UPROPERTY()
	TObjectPtr<ATimeThiefCharacterBase> OwnerCharacter;
};
