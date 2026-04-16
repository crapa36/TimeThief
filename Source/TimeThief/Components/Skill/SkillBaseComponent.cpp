// Fill out your copyright notice in the Description page of Project Settings.


#include "SkillBaseComponent.h"

#include "Character/TimeThiefCharacterBase.h"


// Sets default values for this component's properties
USkillBaseComponent::USkillBaseComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
}


// Called when the game starts
void USkillBaseComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}

void USkillBaseComponent::OnRegister()
{
	Super::OnRegister();
	
	OwnerCharacter = Cast<ATimeThiefCharacterBase>(GetOwner());
}


// Called every frame
void USkillBaseComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                        FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (LeftCoolTime != 0)
	{
		LeftCoolTime = FMath::Max(0, LeftCoolTime - DeltaTime);
	}
}

bool USkillBaseComponent::CanActivate() const
{
	return bCanActivate && LeftCoolTime == 0;
}

