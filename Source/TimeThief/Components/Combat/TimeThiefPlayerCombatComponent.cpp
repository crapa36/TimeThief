#include "Components/Combat/TimeThiefPlayerCombatComponent.h"
#include "Weapon/TimeThiefWeaponBase.h"
#include "Weapon/TimeThiefRifle.h"
#include "TimeThiefGameplayTags.h"
#include "GameFramework/Character.h"

void UTimeThiefPlayerCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	for (const TSubclassOf<ATimeThiefWeaponBase>& WeaponClass : DefaultWeaponClasses)
	{
		if (WeaponClass)
		{
			SpawnAndRegisterWeapon(WeaponClass, false);
		}
	}
}

ATimeThiefWeaponBase* UTimeThiefPlayerCombatComponent::SpawnAndRegisterWeapon(TSubclassOf<ATimeThiefWeaponBase> WeaponClass, bool bEquipImmediately)
{
	if (!WeaponClass)
	{
		return nullptr;
	}

	ACharacter* OwningCharacter = GetPawn<ACharacter>();
	if (!OwningCharacter)
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OwningCharacter;
	SpawnParams.Instigator = OwningCharacter;

	ATimeThiefWeaponBase* SpawnedWeapon = World->SpawnActor<ATimeThiefWeaponBase>(WeaponClass, SpawnParams);
	if (!SpawnedWeapon)
	{
		return nullptr;
	}

	FGameplayTag WeaponTag = SpawnedWeapon->GetWeaponTag();
	RegisterSpawnedWeapon(WeaponTag, SpawnedWeapon, bEquipImmediately);

	if (!bEquipImmediately)
	{
		SpawnedWeapon->SetActorHiddenInGame(true);
	}

	return SpawnedWeapon;
}

void UTimeThiefPlayerCombatComponent::HandleInputPressed(FGameplayTag InputTag)
{
	const FTimeThiefGameplayTags& Tags = FTimeThiefGameplayTags::Get();

	if (InputTag == Tags.InputTag_Action_EquipRifle)
	{
		if (CurrentEquippedWeaponTag == Tags.Weapon_Rifle)
		{
			UnequipCurrentWeapon();
		}
		else
		{
			EquipWeapon(Tags.Weapon_Rifle);
		}
		return;
	}

	if (InputTag == Tags.InputTag_Action_Fire)
	{
		if (ATimeThiefRifle* Rifle = Cast<ATimeThiefRifle>(CurrentEquippedWeapon))
		{
			Rifle->StartFire();
		}
		return;
	}

	if (InputTag == Tags.InputTag_Action_Reload)
	{
		if (ATimeThiefRifle* Rifle = Cast<ATimeThiefRifle>(CurrentEquippedWeapon))
		{
			Rifle->Reload();
		}
		return;
	}

	Super::HandleInputPressed(InputTag);
}

void UTimeThiefPlayerCombatComponent::HandleInputReleased(FGameplayTag InputTag)
{
	const FTimeThiefGameplayTags& Tags = FTimeThiefGameplayTags::Get();

	if (InputTag == Tags.InputTag_Action_Fire)
	{
		if (ATimeThiefRifle* Rifle = Cast<ATimeThiefRifle>(CurrentEquippedWeapon))
		{
			Rifle->StopFire();
		}
		return;
	}

	Super::HandleInputReleased(InputTag);
}

