#include "Components/Combat/TimeThiefPlayerCombatComponent.h"
#include "Weapon/TimeThiefWeaponBase.h"
#include "TimeThiefGameplayTags.h"
#include "Character/TimeThiefCharacterBase.h"
#include "Character/TimeThiefPlayerCharacter.h"
#include "Components/Wire/TimeThiefWireComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraComponent.h"

UTimeThiefPlayerCombatComponent::UTimeThiefPlayerCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UTimeThiefPlayerCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	const FTimeThiefGameplayTags& Tags = FTimeThiefGameplayTags::Get();

	if (WeaponToStateTagMap.Num() == 0)
	{
		WeaponToStateTagMap.Add(Tags.Weapon_Rifle, Tags.State_Combat_Rifle);
		WeaponToStateTagMap.Add(Tags.Weapon_Shotgun, Tags.State_Combat_Shotgun);
		WeaponToStateTagMap.Add(Tags.Weapon_Pistol, Tags.State_Combat_Pistol);
	}

	if (ACharacter* OwningCharacter = GetPawn<ACharacter>())
	{
		if (UCharacterMovementComponent* MovementComp = OwningCharacter->GetCharacterMovement())
		{
			DefaultMaxWalkSpeed = MovementComp->MaxWalkSpeed;
			DefaultRotationRate = MovementComp->RotationRate;
			bDefaultOrientRotationToMovement = MovementComp->bOrientRotationToMovement;
			bDefaultUseControllerDesiredRotation = MovementComp->bUseControllerDesiredRotation;
		}
		bDefaultUseControllerRotationYaw = OwningCharacter->bUseControllerRotationYaw;
		CachedWireComponent = OwningCharacter->FindComponentByClass<UTimeThiefWireComponent>();

		if (const ATimeThiefPlayerCharacter* PlayerChar = Cast<ATimeThiefPlayerCharacter>(OwningCharacter))
		{
			CachedThirdPersonCamera = PlayerChar->GetFollowCamera();
		}
		else
		{
			CachedThirdPersonCamera = OwningCharacter->FindComponentByClass<UCameraComponent>();
		}

		if (ATimeThiefCharacterBase* BaseChar = Cast<ATimeThiefCharacterBase>(OwningCharacter))
		{
			CachedFirstPersonCamera = BaseChar->GetFirstPersonCamera();
		}
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
			StopAiming();
			UnequipCurrentWeapon();
		}
		else
		{
			EquipWeapon(Tags.Weapon_Rifle);
		}
		return;
	}

	if (InputTag == Tags.InputTag_Action_EquipShotgun)
	{
		if (CurrentEquippedWeaponTag == Tags.Weapon_Shotgun)
		{
			StopAiming();
			UnequipCurrentWeapon();
		}
		else
		{
			EquipWeapon(Tags.Weapon_Shotgun);
		}
		return;
	}

	if (InputTag == Tags.InputTag_Action_Fire)
	{
		if (CurrentEquippedWeapon)
		{
			SnapRotationToAim();
			CurrentEquippedWeapon->StartFire();
			UpdateCombatRotation();
		}
		return;
	}

	if (InputTag == Tags.InputTag_Action_Reload)
	{
		if (CurrentEquippedWeapon)
		{
			CurrentEquippedWeapon->Reload();
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
		if (CurrentEquippedWeapon)
		{
			CurrentEquippedWeapon->StopFire();
			UpdateCombatRotation();
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

	SnapRotationToAim();

	if (ACharacter* OwningCharacter = GetPawn<ACharacter>())
	{
		if (UCharacterMovementComponent* MovementComp = OwningCharacter->GetCharacterMovement())
		{
			MovementComp->MaxWalkSpeed = DefaultMaxWalkSpeed * AimMovementSpeedMultiplier;
		}
	}

	UpdateCombatRotation();
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

	UpdateCombatRotation();
}

void UTimeThiefPlayerCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateCombatRotation();
	UpdateAimFOV(DeltaTime);
}

bool UTimeThiefPlayerCombatComponent::IsFiringWeapon() const
{
	return CurrentEquippedWeapon && CurrentEquippedWeapon->IsFiring();
}

bool UTimeThiefPlayerCombatComponent::ShouldUseWeaponControlRigRotation() const
{
	return bIsAiming || IsFiringWeapon();
}

void UTimeThiefPlayerCombatComponent::SnapRotationToAim()
{
	ACharacter* OwningCharacter = GetPawn<ACharacter>();
	if (!OwningCharacter)
	{
		return;
	}

	if (const ATimeThiefCharacterBase* BaseChar = Cast<ATimeThiefCharacterBase>(OwningCharacter))
	{
		if (BaseChar->IsFirstPerson())
		{
			return;
		}
	}

	if (IsRotationManagedExternally())
	{
		return;
	}

	const AController* Controller = OwningCharacter->GetController();
	if (!Controller)
	{
		return;
	}

	const FRotator ControlRotation = Controller->GetControlRotation();
	const FRotator NewRotation(0.0f, ControlRotation.Yaw, 0.0f);
	OwningCharacter->SetActorRotation(NewRotation);
}

void UTimeThiefPlayerCombatComponent::UpdateCombatRotation()
{
	ACharacter* OwningCharacter = GetPawn<ACharacter>();
	if (!OwningCharacter)
	{
		return;
	}

	if (const ATimeThiefCharacterBase* BaseChar = Cast<ATimeThiefCharacterBase>(OwningCharacter))
	{
		if (BaseChar->IsFirstPerson())
		{
			return;
		}
	}

	if (IsRotationManagedExternally())
	{
		return;
	}

	UCharacterMovementComponent* MovementComp = OwningCharacter->GetCharacterMovement();
	if (!MovementComp)
	{
		return;
	}

	ApplyCombatRotationMode(ShouldUseControllerFacing());
}

void UTimeThiefPlayerCombatComponent::ApplyCombatRotationMode(bool bUseControllerFacing)
{
	ACharacter* OwningCharacter = GetPawn<ACharacter>();
	if (!OwningCharacter)
	{
		return;
	}

	UCharacterMovementComponent* MovementComp = OwningCharacter->GetCharacterMovement();
	if (!MovementComp)
	{
		return;
	}

	if (bUseControllerFacing)
	{
		OwningCharacter->bUseControllerRotationYaw = false;
		MovementComp->bOrientRotationToMovement = false;
		MovementComp->bUseControllerDesiredRotation = true;
		MovementComp->RotationRate = FRotator(0.0f, CombatRotationRate, 0.0f);
		return;
	}

	OwningCharacter->bUseControllerRotationYaw = bDefaultUseControllerRotationYaw;
	MovementComp->bOrientRotationToMovement = bDefaultOrientRotationToMovement;
	MovementComp->bUseControllerDesiredRotation = bDefaultUseControllerDesiredRotation;
	MovementComp->RotationRate = DefaultRotationRate;
}

bool UTimeThiefPlayerCombatComponent::ShouldUseControllerFacing() const
{
	const ACharacter* OwningCharacter = GetPawn<ACharacter>();
	if (!OwningCharacter)
	{
		return ShouldUseWeaponControlRigRotation();
	}

	const UCharacterMovementComponent* MovementComp = OwningCharacter->GetCharacterMovement();
	return ShouldUseWeaponControlRigRotation() || HasMovementIntent(MovementComp);
}

bool UTimeThiefPlayerCombatComponent::HasMovementIntent(const UCharacterMovementComponent* MovementComp) const
{
	if (!MovementComp)
	{
		return false;
	}

	const bool bHasAcceleration = !MovementComp->GetCurrentAcceleration().IsNearlyZero();
	const FVector HorizontalVelocity(MovementComp->Velocity.X, MovementComp->Velocity.Y, 0.0f);
	const bool bHasHorizontalVelocity = HorizontalVelocity.SizeSquared() > FMath::Square(25.0f);

	return bHasAcceleration || bHasHorizontalVelocity;
}

bool UTimeThiefPlayerCombatComponent::IsRotationManagedExternally() const
{
	return CachedWireComponent && CachedWireComponent->IsWireActive();
}

void UTimeThiefPlayerCombatComponent::UpdateAimFOV(float DeltaTime)
{
	const float TargetFOV = bIsAiming ? AimFOV : DefaultFOV;

	if (CachedThirdPersonCamera)
	{
		if (!FMath::IsNearlyEqual(CachedThirdPersonCamera->FieldOfView, TargetFOV, 0.1f))
		{
			const float NewFOV = FMath::FInterpTo(CachedThirdPersonCamera->FieldOfView, TargetFOV, DeltaTime, AimInterpSpeed);
			CachedThirdPersonCamera->SetFieldOfView(NewFOV);
		}
	}

	if (CachedFirstPersonCamera)
	{
		if (!FMath::IsNearlyEqual(CachedFirstPersonCamera->FieldOfView, TargetFOV, 0.1f))
		{
			const float NewFOV = FMath::FInterpTo(CachedFirstPersonCamera->FieldOfView, TargetFOV, DeltaTime, AimInterpSpeed);
			CachedFirstPersonCamera->SetFieldOfView(NewFOV);
		}
	}
}