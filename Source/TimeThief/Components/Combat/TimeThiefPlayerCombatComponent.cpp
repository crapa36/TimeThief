#include "Components/Combat/TimeThiefPlayerCombatComponent.h"
#include "Weapon/TimeThiefWeaponBase.h"
#include "Weapon/TimeThiefRifle.h"
#include "TimeThiefGameplayTags.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"

void UTimeThiefPlayerCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	const FTimeThiefGameplayTags& Tags = FTimeThiefGameplayTags::Get();

	if (WeaponToStateTagMap.Num() == 0)
	{
		WeaponToStateTagMap.Add(Tags.Weapon_Rifle, Tags.State_Combat_Rifle);
		WeaponToStateTagMap.Add(Tags.Weapon_Pistol, Tags.State_Combat_Pistol);
	}

	if (ACharacter* OwningCharacter = GetPawn<ACharacter>())
	{
		if (UCharacterMovementComponent* MovementComp = OwningCharacter->GetCharacterMovement())
		{
			DefaultMaxWalkSpeed = MovementComp->MaxWalkSpeed;
		}
		CachedCameraBoom = OwningCharacter->FindComponentByClass<USpringArmComponent>();
	}

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

	if (InputTag == Tags.InputTag_Action_Aim)
	{
		StartAiming();
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

	if (InputTag == Tags.InputTag_Action_Aim)
	{
		StopAiming();
		return;
	}

	Super::HandleInputReleased(InputTag);
}

void UTimeThiefPlayerCombatComponent::StartAiming()
{
	if (bIsAiming || !CurrentEquippedWeapon)
	{
		return;
	}

	bIsAiming = true;

	if (ACharacter* OwningCharacter = GetPawn<ACharacter>())
	{
		if (UCharacterMovementComponent* MovementComp = OwningCharacter->GetCharacterMovement())
		{
			MovementComp->MaxWalkSpeed = DefaultMaxWalkSpeed * AimMovementSpeedMultiplier;
		}
	}
}

void UTimeThiefPlayerCombatComponent::StopAiming()
{
	if (!bIsAiming)
	{
		return;
	}

	bIsAiming = false;

	if (ACharacter* OwningCharacter = GetPawn<ACharacter>())
	{
		if (UCharacterMovementComponent* MovementComp = OwningCharacter->GetCharacterMovement())
		{
			MovementComp->MaxWalkSpeed = DefaultMaxWalkSpeed;
		}
	}
}

void UTimeThiefPlayerCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!CachedCameraBoom)
	{
		return;
	}

	const float TargetFOV = bIsAiming ? AimFOV : DefaultFOV;

	if (UCameraComponent* Camera = Cast<UCameraComponent>(CachedCameraBoom->GetChildComponent(0)))
	{
		const float CurrentFOV = Camera->FieldOfView;
		const float NewFOV = FMath::FInterpTo(CurrentFOV, TargetFOV, DeltaTime, AimInterpSpeed);
		Camera->SetFieldOfView(NewFOV);
	}
}
