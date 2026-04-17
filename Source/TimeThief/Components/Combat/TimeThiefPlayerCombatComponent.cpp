#include "Components/Combat/TimeThiefPlayerCombatComponent.h"
#include "Weapon/TimeThiefMasterWeapon.h"
#include "Weapon/Components/TimeThiefWeaponComponentBase.h"
#include "TimeThiefGameplayTags.h"
#include "Character/TimeThiefCharacterBase.h"
#include "Character/TimeThiefPlayerCharacter.h"
#include "Character/TimeThiefPlayerState.h"
#include "Components/Wire/TimeThiefWireComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Engine/World.h"
#include "Network/State/CombatAttackRequest.h"
#include "Network/MovableNetworkEntityInterface.h"
#include "Network/NetworkMoveComponent.h"
#include "Network/State/CombatNotifyType.h"

UTimeThiefPlayerCombatComponent::UTimeThiefPlayerCombatComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
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
		WeaponToStateTagMap.Add(Tags.Weapon_RocketLauncher, Tags.State_Combat_RocketLauncher);
	}

	if (InputToWeaponTagMap.Num() == 0)
	{
		InputToWeaponTagMap.Add(Tags.InputTag_Action_EquipRifle, Tags.Weapon_Rifle);
		InputToWeaponTagMap.Add(Tags.InputTag_Action_EquipShotgun, Tags.Weapon_Shotgun);
		InputToWeaponTagMap.Add(Tags.InputTag_Action_EquipRocketLauncher, Tags.Weapon_RocketLauncher);
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
}

void UTimeThiefPlayerCombatComponent::Remote_SyncAimLocation(const FVector& Origin, const FVector& Direction)
{
	Super::Remote_SyncAimLocation(Origin, Direction);

	ACharacter* OwningCharacter = GetPawn<ACharacter>();
	if (!OwningCharacter)
	{
		return;
	}

	if (!Direction.IsNearlyZero())
	{
		CachedWorldAimLocation = Origin + Direction.GetSafeNormal() * 50000.0f;
	}
	else
	{
		FVector StartLoc = OwningCharacter->GetActorLocation();
		if (MasterWeaponPtr)
		{
			StartLoc = MasterWeaponPtr->GetActorLocation();
		}
		CachedWorldAimLocation = StartLoc + OwningCharacter->GetBaseAimRotation().Vector() * 50000.0f;
	}

	FVector StartLoc = OwningCharacter->GetActorLocation();
	if (MasterWeaponPtr)
	{
		StartLoc = MasterWeaponPtr->GetActorLocation();
	}

	const FVector AimVector = CachedWorldAimLocation - StartLoc;
	if (AimVector.IsNearlyZero())
	{
		return;
	}

	const FRotator AimRotation = AimVector.GetSafeNormal().Rotation();
	OwningCharacter->SetActorRotation(FRotator(0.0f, AimRotation.Yaw, 0.0f));

	if (AController* OwnerController = OwningCharacter->GetController())
	{
		OwnerController->SetControlRotation(AimRotation);
	}

	if (IMovableNetworkEntityInterface* Movable = Cast<IMovableNetworkEntityInterface>(OwningCharacter))
	{
		Movable->SetNetworkPitch(AimRotation.Pitch);
		Movable->SetNetworkYaw(AimRotation.Yaw);
	}
}

void UTimeThiefPlayerCombatComponent::HandleInputPressed(FGameplayTag InputTag)
{
	const FTimeThiefGameplayTags& Tags = FTimeThiefGameplayTags::Get();

	if (const FGameplayTag* WeaponTag = InputToWeaponTagMap.Find(InputTag))
	{
		if (CurrentEquippedWeaponTag == *WeaponTag)
		{
			StopAiming();
			UnequipCurrentWeapon();
		}
		else
		{
			const FGameplayTag PreviousWeaponTag = CurrentEquippedWeaponTag;
			EquipWeapon(*WeaponTag);

			if (CurrentEquippedWeaponTag.IsValid() && CurrentEquippedWeaponTag != PreviousWeaponTag)
			{
				FCombatAttackRequest Request{};
				Request.NotifyType = ECombatNotifyType::WeaponChange;
				Request.WeaponId = FTimeThiefGameplayTags::ResolveWeaponIdFromTag(CurrentEquippedWeaponTag);
				BroadcastCombatAttackRequest(Request);
			}
		}
		return;
	}

	if (InputTag == Tags.InputTag_Action_Fire)
	{
		bIsFireInputHeld = true;
		if (MasterWeaponPtr && !bIsEquippingWeapon)
		{
			SnapRotationToAim();
			MasterWeaponPtr->StartFire();
			UpdateCombatRotation();
		}
		return;
	}

	if (InputTag == Tags.InputTag_Action_Reload)
	{
		if (MasterWeaponPtr && !bIsEquippingWeapon)
		{
			MasterWeaponPtr->Reload();
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
		bIsFireInputHeld = false;
		if (MasterWeaponPtr)
		{
			MasterWeaponPtr->StopFire();
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

void UTimeThiefPlayerCombatComponent::EquipWeapon(FGameplayTag WeaponTag)
{
	Super::EquipWeapon(WeaponTag);
	ApplyUpgradeStatsToActiveWeapon();
}

void UTimeThiefPlayerCombatComponent::ApplyUpgradeStatsToActiveWeapon()
{
	if (!MasterWeaponPtr)
	{
		return;
	}

	UTimeThiefWeaponComponentBase* CurrentWeaponComp = MasterWeaponPtr->GetActiveWeaponComponent();
	if (!CurrentWeaponComp)
	{
		return;
	}

	const ACharacter* OwningCharacter = GetPawn<ACharacter>();
	if (!OwningCharacter)
	{
		return;
	}

	const ATimeThiefPlayerState* PS = OwningCharacter->GetPlayerState<ATimeThiefPlayerState>();
	if (!PS)
	{
		return;
	}

	if (const FAppliedWeaponUpgradeStats* WeaponStats = PS->GetAppliedWeaponUpgradeStats(CurrentWeaponComp->GetWeaponTag()))
	{
		CurrentWeaponComp->SetDamageBonus(WeaponStats->DamageBonus);
		CurrentWeaponComp->SetCapacityBonus(WeaponStats->CapacityBonusAmmo);
		CurrentWeaponComp->SetRecoilReduction(WeaponStats->RecoilReduction);
	}
	else
	{
		CurrentWeaponComp->SetDamageBonus(0.0f);
		CurrentWeaponComp->SetCapacityBonus(0);
		CurrentWeaponComp->SetRecoilReduction(0.0f);
	}
}

void UTimeThiefPlayerCombatComponent::OnEquipFinished()
{
	Super::OnEquipFinished();
	ApplyUpgradeStatsToActiveWeapon();

	if (bIsFireInputHeld && MasterWeaponPtr)
	{
		SnapRotationToAim();
		MasterWeaponPtr->StartFire();
		UpdateCombatRotation();
	}
}

void UTimeThiefPlayerCombatComponent::StartAiming()
{
	if (bIsAiming || !MasterWeaponPtr || !MasterWeaponPtr->GetActiveWeaponComponent()) return;

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

	FCombatAttackRequest Request{};
	Request.NotifyType = ECombatNotifyType::Aiming;
	BroadcastCombatAttackRequest(Request);
}

void UTimeThiefPlayerCombatComponent::StopAiming()
{
	if (!bIsAiming) return;

	bIsAiming = false;

	if (ACharacter* OwningCharacter = GetPawn<ACharacter>())
	{
		if (UCharacterMovementComponent* MovementComp = OwningCharacter->GetCharacterMovement())
		{
			MovementComp->MaxWalkSpeed = DefaultMaxWalkSpeed;
		}
	}

	UpdateCombatRotation();

	FCombatAttackRequest Request{};
	Request.NotifyType = ECombatNotifyType::Readying;
	BroadcastCombatAttackRequest(Request);
}

void UTimeThiefPlayerCombatComponent::Local_StartAiming()
{
	bIsAiming = true;
}

void UTimeThiefPlayerCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (IsFiringWeapon())
	{
		if (UWorld* World = GetWorld())
		{
			LastFireTime = World->GetTimeSeconds();
		}
	}

	UpdateWorldAimLocation();
	UpdateCombatRotation();
	
	ACharacter* OwningCharacter = GetPawn<ACharacter>();
	if (OwningCharacter && ShouldUseControllerFacing() && !IsRotationManagedExternally())
	{
		if (const ATimeThiefCharacterBase* BaseChar = Cast<ATimeThiefCharacterBase>(OwningCharacter))
		{
			if (!BaseChar->IsFirstPerson())
			{
				FVector StartLoc = OwningCharacter->GetActorLocation();
				if (MasterWeaponPtr)
				{
					StartLoc = MasterWeaponPtr->GetActorLocation();
				}
				FVector AimDirection = CachedWorldAimLocation - StartLoc;
				AimDirection.Z = 0.0f;
				
				if (AimDirection.SizeSquared() > KINDA_SMALL_NUMBER)
				{
					FRotator TargetRotation = AimDirection.Rotation();
					const float ClampedTargetYaw = GetClampedYawFromCamera(OwningCharacter, TargetRotation.Yaw);
					FRotator CurrentRotation = OwningCharacter->GetActorRotation();
					FRotator NewRotation = FMath::RInterpTo(CurrentRotation, FRotator(0.0f, ClampedTargetYaw, 0.0f), DeltaTime, 20.0f);

					if (OwningCharacter->IsLocallyControlled())
					{
						OwningCharacter->SetActorRotation(NewRotation);
					}
				}
			}
		}
	}

	UpdateAimFOV(DeltaTime);
}

void UTimeThiefPlayerCombatComponent::UpdateWorldAimLocation()
{
	ACharacter* OwningCharacter = GetPawn<ACharacter>();
	if (!OwningCharacter) return;

	const APlayerController* PC = Cast<APlayerController>(OwningCharacter->GetController());
	if (!PC) return;

	FVector CameraLocation;
	FRotator CameraRotation;
	PC->GetPlayerViewPoint(CameraLocation, CameraRotation);

	FVector TraceStart = CameraLocation;
	FVector TraceEnd = TraceStart + (CameraRotation.Vector() * 50000.0f);

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(OwningCharacter);
	if (MasterWeaponPtr)
	{
		QueryParams.AddIgnoredActor(MasterWeaponPtr);
	}

	if (GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
	{
		CachedWorldAimLocation = HitResult.ImpactPoint;
	}
	else
	{
		CachedWorldAimLocation = TraceEnd;
	}
}

bool UTimeThiefPlayerCombatComponent::IsFiringWeapon() const
{
	if (MasterWeaponPtr)
	{
		if (UTimeThiefWeaponComponentBase* ActiveComp = MasterWeaponPtr->GetActiveWeaponComponent())
		{
			return ActiveComp->IsFiring();
		}
	}
	return false;
}

bool UTimeThiefPlayerCombatComponent::ShouldUseWeaponControlRigRotation() const
{
	if (bIsAiming || IsFiringWeapon())
	{
		return true;
	}

	if (UWorld* World = GetWorld())
	{
		return (World->GetTimeSeconds() - LastFireTime) < PostFireRotationDelay;
	}

	return false;
}

void UTimeThiefPlayerCombatComponent::SnapRotationToAim() 
{
	ACharacter* OwningCharacter = GetPawn<ACharacter>();
	if (!OwningCharacter) return;

	if (const ATimeThiefCharacterBase* BaseChar = Cast<ATimeThiefCharacterBase>(OwningCharacter))
	{
		if (BaseChar->IsFirstPerson()) return;
	}

	if (IsRotationManagedExternally()) return;

	UpdateWorldAimLocation();

	FVector StartLoc = OwningCharacter->GetActorLocation();
	if (MasterWeaponPtr)
	{
		StartLoc = MasterWeaponPtr->GetActorLocation();
	}
	
	FVector AimDirection = CachedWorldAimLocation - StartLoc;
	AimDirection.Z = 0.0f;
	
	if (AimDirection.SizeSquared() > KINDA_SMALL_NUMBER)
	{
		FRotator TargetRotation = AimDirection.Rotation();
		const float ClampedTargetYaw = GetClampedYawFromCamera(OwningCharacter, TargetRotation.Yaw);
		
		if (OwningCharacter->IsLocallyControlled())
		{
			OwningCharacter->SetActorRotation(FRotator(0.0f, ClampedTargetYaw, 0.0f));
		}
	}
}

float UTimeThiefPlayerCombatComponent::GetClampedYawFromCamera(const ACharacter* OwningCharacter, float TargetYaw) const
{
	if (const AController* OwnerController = OwningCharacter->GetController())
	{
		const float CameraYaw = OwnerController->GetControlRotation().Yaw;
		const float DeltaYaw = FMath::FindDeltaAngleDegrees(CameraYaw, TargetYaw);
		const float ClampedDeltaYaw = FMath::Clamp(DeltaYaw, -MaxYawOffsetFromCamera, MaxYawOffsetFromCamera);
		return FRotator::NormalizeAxis(CameraYaw + ClampedDeltaYaw);
	}

	return TargetYaw;
}

void UTimeThiefPlayerCombatComponent::UpdateCombatRotation()
{
	ACharacter* OwningCharacter = GetPawn<ACharacter>();
	if (!OwningCharacter) return;

	if (const ATimeThiefCharacterBase* BaseChar = Cast<ATimeThiefCharacterBase>(OwningCharacter))
	{
		if (BaseChar->IsFirstPerson()) return;
	}

	if (IsRotationManagedExternally()) return;

	UCharacterMovementComponent* MovementComp = OwningCharacter->GetCharacterMovement();
	if (!MovementComp) return;

	ApplyCombatRotationMode(ShouldUseControllerFacing());
}

void UTimeThiefPlayerCombatComponent::ApplyCombatRotationMode(bool bUseControllerFacing)
{
	ACharacter* OwningCharacter = GetPawn<ACharacter>();
	if (!OwningCharacter) return;

	UCharacterMovementComponent* MovementComp = OwningCharacter->GetCharacterMovement();
	if (!MovementComp) return;

	if (bUseControllerFacing)
	{
		OwningCharacter->bUseControllerRotationYaw = false;
		MovementComp->bOrientRotationToMovement = false;
		MovementComp->bUseControllerDesiredRotation = false;
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
	if (!OwningCharacter) return ShouldUseWeaponControlRigRotation();

	const UCharacterMovementComponent* MovementComp = OwningCharacter->GetCharacterMovement();
	return ShouldUseWeaponControlRigRotation() || HasMovementIntent(MovementComp);
}

bool UTimeThiefPlayerCombatComponent::HasMovementIntent(const UCharacterMovementComponent* MovementComp) const
{
	if (!MovementComp) return false;

	const bool bHasAcceleration = !MovementComp->GetCurrentAcceleration().IsNearlyZero();
	const FVector HorizontalVelocity(MovementComp->Velocity.X, MovementComp->Velocity.Y, 0.0f);
	const bool bHasHorizontalVelocity = HorizontalVelocity.SizeSquared() > FMath::Square(25.0f);

	return bHasAcceleration || bHasHorizontalVelocity;
}

bool UTimeThiefPlayerCombatComponent::IsRotationManagedExternally() const
{
	if (!CachedWireComponent.IsValid())
	{
		if (AActor* Owner = GetOwner())
		{
			CachedWireComponent = Owner->FindComponentByClass<UTimeThiefWireComponent>();
		}
	}
	return CachedWireComponent.IsValid() && CachedWireComponent->IsWireActive();
}

void UTimeThiefPlayerCombatComponent::UpdateAimFOV(float DeltaTime)
{
	const float TargetFOV = bIsAiming ? AimFOV : DefaultFOV;

	if (CachedThirdPersonCamera.IsValid())
	{
		if (!FMath::IsNearlyEqual(CachedThirdPersonCamera->FieldOfView, TargetFOV, 0.1f))
		{
			const float NewFOV = FMath::FInterpTo(CachedThirdPersonCamera->FieldOfView, TargetFOV, DeltaTime, AimInterpSpeed);
			CachedThirdPersonCamera->SetFieldOfView(NewFOV);
		}
	}

	if (CachedFirstPersonCamera.IsValid())
	{
		if (!FMath::IsNearlyEqual(CachedFirstPersonCamera->FieldOfView, TargetFOV, 0.1f))
		{
			const float NewFOV = FMath::FInterpTo(CachedFirstPersonCamera->FieldOfView, TargetFOV, DeltaTime, AimInterpSpeed);
			CachedFirstPersonCamera->SetFieldOfView(NewFOV);
		}
	}
}

void UTimeThiefPlayerCombatComponent::SetMoveSpeedUpgradeBonus(float InMoveSpeedBonus)
{
	ACharacter* OwningCharacter = GetPawn<ACharacter>();
	if (!OwningCharacter) return;

	UCharacterMovementComponent* MC = OwningCharacter->GetCharacterMovement();
	if (!MC) return;

	float BaseSpeed = DefaultMaxWalkSpeed;
	if (const ATimeThiefPlayerCharacter* PlayerChar = Cast<ATimeThiefPlayerCharacter>(OwningCharacter))
	{
		BaseSpeed = PlayerChar->GetBaseMoveSpeed();
	}
	else if (BaseSpeed <= 0.0f)
	{
		BaseSpeed = MC->MaxWalkSpeed;
	}

	DefaultMaxWalkSpeed = BaseSpeed + InMoveSpeedBonus;

	if (!bIsAiming)
	{
		MC->MaxWalkSpeed = DefaultMaxWalkSpeed;
	}

#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log, TEXT("[Combat][MoveSpeed] Base=%.2f Bonus=%.2f DefaultMaxWalkSpeed=%.2f AppliedMaxWalkSpeed=%.2f bIsAiming=%s"),
		BaseSpeed,
		InMoveSpeedBonus,
		DefaultMaxWalkSpeed,
		MC->MaxWalkSpeed,
		bIsAiming ? TEXT("true") : TEXT("false"));
#endif
}