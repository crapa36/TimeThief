#include "Components/Combat/TimeThiefPawnCombatComponent.h"
#include "Weapon/TimeThiefWeaponBase.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/TimeThiefCharacterBase.h"

UTimeThiefPawnCombatComponent::UTimeThiefPawnCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTimeThiefPawnCombatComponent::RegisterSpawnedWeapon(FGameplayTag InWeaponTagToRegister, ATimeThiefWeaponBase* InWeaponToRegister, bool bRegisterAsEquippedWeapon)
{
	if (!InWeaponToRegister || !InWeaponTagToRegister.IsValid())
	{
		return;
	}

	if (CharacterCarriedWeaponMap.Contains(InWeaponTagToRegister))
	{
		return;
	}

	CharacterCarriedWeaponMap.Emplace(InWeaponTagToRegister, InWeaponToRegister);

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

	CurrentEquippedWeaponTag = WeaponTag;
	CurrentEquippedWeapon = WeaponToEquip;

	WeaponToEquip->SetActorHiddenInGame(false);
	AttachWeaponToSocket(WeaponToEquip);

	if (TSubclassOf<UAnimInstance> AnimLayer = WeaponToEquip->GetEquipAnimLayer())
	{
		OwningCharacter->GetMesh()->LinkAnimClassLayers(AnimLayer);
	}

	PlayEquipMontage(WeaponToEquip);
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
		if (BaseChar->IsLocallyControlled() && BaseChar->GetFirstPersonMesh())
		{
			TargetMesh = BaseChar->GetFirstPersonMesh();
		}
	}

	Weapon->AttachToComponent(TargetMesh, FAttachmentTransformRules::SnapToTargetIncludingScale, SocketToUse);
}

void UTimeThiefPawnCombatComponent::PlayEquipMontage(ATimeThiefWeaponBase* Weapon)
{
	if (!Weapon)
	{
		return;
	}

	UAnimMontage* EquipMontage = Weapon->GetEquipMontage();
	if (!EquipMontage)
	{
		return;
	}

	if (ATimeThiefCharacterBase* BaseChar = Cast<ATimeThiefCharacterBase>(GetPawn<ACharacter>()))
	{
		BaseChar->PlayMontageOnAllMeshes(EquipMontage);
	}
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