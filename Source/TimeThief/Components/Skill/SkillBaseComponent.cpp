// Fill out your copyright notice in the Description page of Project Settings.


#include "SkillBaseComponent.h"

#include "Character/TimeThiefCharacterBase.h"
#include "Engine/World.h"


// Sets default values for this component's properties
USkillBaseComponent::USkillBaseComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void USkillBaseComponent::OnRegister()
{
	Super::OnRegister();
	
	OwnerCharacter = Cast<ATimeThiefCharacterBase>(GetOwner());
}


bool USkillBaseComponent::CanActivate() const
{
	return bCanActivate && GetRemainingCoolTime() <= 0.0f;
}

float USkillBaseComponent::GetRemainingCoolTime() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}

	return FMath::Max(0.0f, CooldownEndTimeSeconds - World->GetTimeSeconds());
}

float USkillBaseComponent::GetCooldownPercent() const
{
	return CoolTime > 0.0f ? FMath::Clamp(GetRemainingCoolTime() / CoolTime, 0.0f, 1.0f) : 0.0f;
}

void USkillBaseComponent::ApplyServerCooldownMs(uint32 RemainingCooldownMs)
{
	StartCooldown(static_cast<float>(RemainingCooldownMs) / 1000.0f);
}

void USkillBaseComponent::StartCooldown(float DurationSeconds)
{
	const float CooldownSeconds = FMath::Max(0.0f, DurationSeconds);
	if (CooldownSeconds <= 0.0f)
	{
		CooldownEndTimeSeconds = 0.0f;
		return;
	}

	const UWorld* World = GetWorld();
	CooldownEndTimeSeconds = World
		? World->GetTimeSeconds() + CooldownSeconds
		: 0.0f;
}

