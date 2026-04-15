// Fill out your copyright notice in the Description page of Project Settings.


#include "SavePointSkillComponent.h"

#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"


// Sets default values for this component's properties
USavePointSkillComponent::USavePointSkillComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	SavePointEffect = CreateDefaultSubobject<UNiagaraComponent>("SavePointEffect");
	SavePointEffect->bAutoActivate = false;
}

// Called when the game starts
void USavePointSkillComponent::BeginPlay()
{
	Super::BeginPlay();

	SavePointEffect->OnSystemFinished.AddDynamic(this, &ThisClass::OnFinished);
}

void USavePointSkillComponent::OnRegister()
{
	Super::OnRegister();
	
	if (auto Owner = Cast<ACharacter>(GetOwner()))
	{
		SavePointEffect->AttachToComponent(Owner->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, NAME_None);
	}
}

// Called every frame
void USavePointSkillComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                             FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void USavePointSkillComponent::ActivateSkill()
{
	UKismetSystemLibrary::PrintString(this, TEXT("Save Point Skill Activated"));
	SavePointEffect->Activate(true);
	if (auto Character = Cast<ACharacter>(GetOwner()))
	{
		if (UAnimInstance* ThirdPersonAnim = Character->GetMesh()->GetAnimInstance())
		{
			ThirdPersonAnim->Montage_Play(Montage);
		}
	}
	
	if (auto Owner = Cast<ACharacter>(GetOwner()))
	{
		if (auto PC = Cast<APlayerController>(Owner->GetController()))
		{
			Owner->DisableInput(PC);
		}
		Owner->GetCharacterMovement()->StopMovementImmediately();
	}
	LeftCoolTime = CoolTime;
}

void USavePointSkillComponent::OnFinished(UNiagaraComponent* FinishedComponent)
{
	if (auto Character = Cast<ACharacter>(GetOwner()))
	{
		if (auto PC = Cast<APlayerController>(Character->GetController()))
		{
			Character->EnableInput(PC);
		}
	}
}

