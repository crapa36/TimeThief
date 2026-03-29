#include "Components/Combat/TimeThiefPawnCombatComponent.h"
#include "Weapon/TimeThiefWeaponBase.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/TimeThiefCharacterBase.h"
#include "Engine/World.h"
#include "TimerManager.h"

UTimeThiefPawnCombatComponent::UTimeThiefPawnCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UTimeThiefPawnCombatComponent::RegisterSpawnedWeapon(FGameplayTag InWeaponTagToRegister, ATimeThiefWeaponBase* InWeaponToRegister, bool bRegisterAsEquippedWeapon)
{
	if (!InWeaponToRegister || !InWeaponTagToRegister.IsValid())
	{
		return;
	}

	TObjectPtr<ATimeThiefWeaponBase>& RegisteredWeapon = CharacterCarriedWeaponMap.FindOrAdd(InWeaponTagToRegister);
	if (RegisteredWeapon)
	{
		return;
	}

	RegisteredWeapon = InWeaponToRegister;

	if (bRegisterAsEquippedWeapon)
	{
		EquipWeapon(InWeaponTagToRegister);
	}
}

void UTimeThiefPawnCombatComponent::EquipWeapon(FGameplayTag WeaponTag)
{
	ATimeThiefWeaponBase* WeaponToEquip = GetCharacterCarriedWeaponByTag(WeaponTag);
	if (!WeaponToEquip)
	{
		return;
	}

	if (CurrentEquippedWeaponTag == WeaponTag)
	{
		return;
	}

	ACharacter* OwningCharacter = GetPawn<ACharacter>();
	if (!OwningCharacter)
	{
		return;
	}

	if (CurrentEquippedWeapon)
	{
		UnequipCurrentWeapon();
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EquipTimerHandle);
	}

	CurrentEquippedWeaponTag = WeaponTag;
	CurrentEquippedWeapon = WeaponToEquip;

	WeaponToEquip->SetActorHiddenInGame(false);
	AttachWeaponToSocket(WeaponToEquip);

	if (TSubclassOf<UAnimInstance> AnimLayer = WeaponToEquip->GetEquipAnimLayer())
	{
		OwningCharacter->GetMesh()->LinkAnimClassLayers(AnimLayer);
		if (ATimeThiefCharacterBase* BaseChar = Cast<ATimeThiefCharacterBase>(OwningCharacter))
		{
			if (USkeletalMeshComponent* FPMesh = BaseChar->GetFirstPersonMesh())
			{
				FPMesh->LinkAnimClassLayers(AnimLayer);
			}
		}
	}

	const float MontageLength = PlayEquipMontage(WeaponToEquip);
	if (MontageLength > 0.0f)
	{
		bIsEquippingWeapon = true;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(EquipTimerHandle, this, &UTimeThiefPawnCombatComponent::OnEquipFinished, MontageLength, false);
		}
	}
	else
	{
		OnEquipFinished();
	}

	ApplyCombatStateTag(WeaponTag);
	OnWeaponEquipped_Delegate.Broadcast(WeaponToEquip);
}

void UTimeThiefPawnCombatComponent::UnequipCurrentWeapon()
{
	if (!CurrentEquippedWeapon)
	{
		return;
	}

	ACharacter* OwningCharacter = GetPawn<ACharacter>();
	if (!OwningCharacter)
	{
		return;
	}

	if (UAnimMontage* UnequipMontage = CurrentEquippedWeapon->GetUnequipMontage())
	{
		if (ATimeThiefCharacterBase* BaseChar = Cast<ATimeThiefCharacterBase>(OwningCharacter))
		{
			BaseChar->PlayMontageOnAllMeshes(UnequipMontage);
		}
	}

	CurrentEquippedWeapon->SetActorHiddenInGame(true);

	if (TSubclassOf<UAnimInstance> AnimLayer = CurrentEquippedWeapon->GetEquipAnimLayer())
	{
		OwningCharacter->GetMesh()->UnlinkAnimClassLayers(AnimLayer);
		if (ATimeThiefCharacterBase* BaseChar = Cast<ATimeThiefCharacterBase>(OwningCharacter))
		{
			if (USkeletalMeshComponent* FPMesh = BaseChar->GetFirstPersonMesh())
			{
				FPMesh->UnlinkAnimClassLayers(AnimLayer);
			}
		}
	}

	RemoveCombatStateTag(CurrentEquippedWeaponTag);
	CurrentEquippedWeaponTag = FGameplayTag();
	CurrentEquippedWeapon = nullptr;

	OnWeaponUnequipped_Delegate.Broadcast();
}

void UTimeThiefPawnCombatComponent::AttachWeaponToSocket(ATimeThiefWeaponBase* Weapon)
{
	if (!Weapon)
	{
		return;
	}

	ACharacter* OwningCharacter = GetPawn<ACharacter>();
	if (!OwningCharacter)
	{
		return;
	}

	FName SocketToUse = Weapon->GetSocketName();
	USkeletalMeshComponent* TargetMesh = OwningCharacter->GetMesh();

	if (ATimeThiefCharacterBase* BaseChar = Cast<ATimeThiefCharacterBase>(OwningCharacter))
	{
		USkeletalMeshComponent* AttachMesh = BaseChar->GetWeaponAttachMesh();
		if (AttachMesh)
		{
			TargetMesh = AttachMesh;
		}
	}

	Weapon->AttachToComponent(TargetMesh, FAttachmentTransformRules::SnapToTargetIncludingScale, SocketToUse);
}

void UTimeThiefPawnCombatComponent::PlayFireMontage()
{
	if (ATimeThiefCharacterBase* Character = Cast<ATimeThiefCharacterBase>(GetOwner()))
	{
		Character->PlayMontageOnAllMeshes(FireMontage);
	}
}

float UTimeThiefPawnCombatComponent::PlayEquipMontage(ATimeThiefWeaponBase* Weapon)
{
	if (!Weapon)
	{
		return 0.0f;
	}

	UAnimMontage* EquipMontage = Weapon->GetEquipMontage();
	if (!EquipMontage)
	{
		return 0.0f;
	}

	if (ATimeThiefCharacterBase* BaseChar = Cast<ATimeThiefCharacterBase>(GetPawn<ACharacter>()))
	{
		BaseChar->PlayMontageOnAllMeshes(EquipMontage);
		return EquipMontage->GetPlayLength();
	}

	return 0.0f;
}

void UTimeThiefPawnCombatComponent::OnEquipFinished()
{
	bIsEquippingWeapon = false;
}

ATimeThiefWeaponBase* UTimeThiefPawnCombatComponent::GetCharacterCarriedWeaponByTag(FGameplayTag InWeaponTagToGet) const
{
	if (const TObjectPtr<ATimeThiefWeaponBase>* FoundWeapon = CharacterCarriedWeaponMap.Find(InWeaponTagToGet))
	{
		return *FoundWeapon;
	}
	return nullptr;
}

ATimeThiefWeaponBase* UTimeThiefPawnCombatComponent::GetCharacterCurrentEquippedWeapon() const
{
	return CurrentEquippedWeapon;
}

void UTimeThiefPawnCombatComponent::HandleInputPressed(FGameplayTag InputTag)
{
}

void UTimeThiefPawnCombatComponent::HandleInputReleased(FGameplayTag InputTag)
{
}

void UTimeThiefPawnCombatComponent::ApplyCombatStateTag(FGameplayTag WeaponTag)
{
	if (!WeaponTag.IsValid()) return;

	if (const FGameplayTag* StateTag = WeaponToStateTagMap.Find(WeaponTag))
	{
		if (ATimeThiefCharacterBase* BaseChar = Cast<ATimeThiefCharacterBase>(GetPawn<ACharacter>()))
		{
			BaseChar->AddOwnedGameplayTag(*StateTag);
		}
	}
}

void UTimeThiefPawnCombatComponent::RemoveCombatStateTag(FGameplayTag WeaponTag)
{
	if (!WeaponTag.IsValid()) return;

	if (const FGameplayTag* StateTag = WeaponToStateTagMap.Find(WeaponTag))
	{
		if (ATimeThiefCharacterBase* BaseChar = Cast<ATimeThiefCharacterBase>(GetPawn<ACharacter>()))
		{
			BaseChar->RemoveOwnedGameplayTag(*StateTag);
		}
	}
}

void UTimeThiefPawnCombatComponent::Remote_SyncAimingState(bool bNewAiming)
{
	bIsAiming = bNewAiming;
}

void UTimeThiefPawnCombatComponent::Remote_SyncFireAction()
{
	PlayFireMontage();
}

void UTimeThiefPawnCombatComponent::Remote_SyncAimLocation(const FVector& NewAimLocation)
{
	RemoteTargetAimLocation = NewAimLocation;
}

void UTimeThiefPawnCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TargetAimLocation = FMath::VInterpTo(TargetAimLocation, RemoteTargetAimLocation, DeltaTime, 15.f);
}