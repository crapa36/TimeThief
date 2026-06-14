// Fill out your copyright notice in the Description page of Project Settings.


#include "SavePointSkillComponent.h"

#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Character/TimeThiefCharacterBase.h"
#include "Character/TimeThiefPlayerCharacter.h"
#include "Components/System/InventorySystemComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"


void USavePointSkillComponent::OnBeginRespawn()
{
	ILifeObserver::OnBeginRespawn();
	
	if (auto PS = Cast<ATimeThiefPlayerState>(OwnerCharacter->GetPlayerState()))
	{
		PS->Status = SavedStatus;
	}
	
	OwnerCharacter->SetActorLocation(SavedLocation);
	
	if (auto Player = Cast<ATimeThiefPlayerCharacter>(OwnerCharacter))
	{
		Player->GetInventoryComponent()->SetInventory(SavedInventory);
	}
}

// Sets default values for this component's properties
USavePointSkillComponent::USavePointSkillComponent()
{
	SavePointEffect = CreateDefaultSubobject<UNiagaraComponent>("SavePointEffect");
	SavePointEffect->bAutoActivate = false;
	
	bCanActivate = true;
}

// Called when the game starts
void USavePointSkillComponent::BeginPlay()
{
	Super::BeginPlay();

	SavePointEffect->OnSystemFinished.AddDynamic(this, &ThisClass::OnFinished);
	
	Save();
}

void USavePointSkillComponent::OnRegister()
{
	Super::OnRegister();
	
	if (OwnerCharacter)
	{
		SavePointEffect->AttachToComponent(OwnerCharacter->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, NAME_None);
	}
}

void USavePointSkillComponent::ActivateSkill()
{
	if (!CanActivate())
	{
		return;
	}
	
	Save();
	
	SavePointEffect->Activate(true);
	
	if (OwnerCharacter)
	{
		if (UAnimInstance* ThirdPersonAnim = OwnerCharacter->GetMesh()->GetAnimInstance())
		{
			ThirdPersonAnim->Montage_Play(Montage);
		}
	}
	
	if (OwnerCharacter)
	{
		if (auto PC = Cast<APlayerController>(OwnerCharacter->GetController()))
		{
			OwnerCharacter->DisableInput(PC);
		}
		OwnerCharacter->GetCharacterMovement()->StopMovementImmediately();
	}
	
	StartCooldown(CoolTime);
}

void USavePointSkillComponent::OnFinished(UNiagaraComponent* FinishedComponent)
{
	if (OwnerCharacter)
	{
		if (auto PC = Cast<APlayerController>(OwnerCharacter->GetController()))
		{
			OwnerCharacter->EnableInput(PC);
		}
	}
	
	if (OwnerCharacter)
	{
		if (UAnimInstance* ThirdPersonAnim = OwnerCharacter->GetMesh()->GetAnimInstance())
		{
			ThirdPersonAnim->Montage_Stop(0.2f, Montage);
		}
	}
}

void USavePointSkillComponent::Save()
{
	SavedLocation = OwnerCharacter->GetActorLocation();
	
	if (auto PS = Cast<ATimeThiefPlayerState>(OwnerCharacter->GetPlayerState()))
	{
		SavedStatus = PS->Status;
	}
	
	if (auto InventoryComp = OwnerCharacter->GetComponentByClass<UInventorySystemComponent>())
	{
		const auto& Inventory = InventoryComp->GetInventory();
		SavedInventory.Reset(Inventory.Num());
		for (auto p : Inventory)
		{
			if (p)
			{
				SavedInventory.Emplace(p->ItemID, p->Quantity);
			}
		}
	}
}

