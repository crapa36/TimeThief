#include "Components/Combat/TimeThiefPlayerCombatComponent.h"
#include "Weapon/TimeThiefMasterWeapon.h"
#include "Weapon/Components/TimeThiefWeaponComponentBase.h"
#include "TimeThiefGameplayTags.h"
#include "Character/TimeThiefCharacterBase.h"
#include "Character/TimeThiefPlayerCharacter.h"
#include "Character/TimeThiefPlayerState.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Engine/World.h"
#include "Network/State/CombatAttackRequest.h"
#include "Network/MovableNetworkEntityInterface.h"
#include "Network/State/CombatNotifyType.h"
#include "Utils/TimeThiefAimStatics.h"

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
			MovementComp->bOrientRotationToMovement = false;
		}

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

	if (const APawn* OwningPawn = GetPawn<APawn>(); OwningPawn && OwningPawn->IsLocallyControlled())
	{
		EquipWeapon(Tags.Weapon_Rifle);
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

	if (const APawn* OwningPawn = Cast<APawn>(OwningCharacter))
	{
		if (OwningPawn->IsLocallyControlled())
		{
			return;
		}
	}

	if (!Direction.IsNearlyZero())
	{
		CachedWorldAimLocation = UTimeThiefAimStatics::ResolveAimTargetLocation(Origin, Direction, AimTraceRange);
	}
	else
	{
		FVector ViewLocation = FVector::ZeroVector;
		FVector ViewDirection = FVector::ForwardVector;
		if (const APawn* OwningPawn = Cast<APawn>(OwningCharacter);
			OwningPawn && UTimeThiefAimStatics::ResolveAimView(OwningPawn, ViewLocation, ViewDirection))
		{
			CachedWorldAimLocation = UTimeThiefAimStatics::ResolveAimTargetLocation(
				GetEffectiveShotOrigin(),
				ViewDirection,
				AimTraceRange);
		}
		else
		{
			CachedWorldAimLocation = UTimeThiefAimStatics::ResolveAimTargetLocation(
				GetEffectiveShotOrigin(),
				OwningCharacter->GetActorForwardVector(),
				AimTraceRange);
		}
	}

	if (IMovableNetworkEntityInterface* Movable = Cast<IMovableNetworkEntityInterface>(OwningCharacter))
	{
		const FVector AimDirection = UTimeThiefAimStatics::ResolveAimDirectionToTarget(
			GetEffectiveShotOrigin(),
			CachedWorldAimLocation,
			OwningCharacter->GetActorForwardVector());
		const FRotator AimRotation = UTimeThiefAimStatics::ResolveAimRotationFromDirection(
			AimDirection,
			OwningCharacter->GetActorRotation());
		Movable->SetNetworkYaw(AimRotation.Yaw);
		Movable->SetNetworkPitch(AimRotation.Pitch);
	}
}

void UTimeThiefPlayerCombatComponent::HandleInputPressed(FGameplayTag InputTag)
{
	const FTimeThiefGameplayTags& Tags = FTimeThiefGameplayTags::Get();

	if (const FGameplayTag* WeaponTag = InputToWeaponTagMap.Find(InputTag))
	{
		if (CurrentEquippedWeaponTag == *WeaponTag)
		{
			return;
		}

		const FGameplayTag PreviousWeaponTag = CurrentEquippedWeaponTag;
		EquipWeapon(*WeaponTag);

		if (CurrentEquippedWeaponTag.IsValid() && CurrentEquippedWeaponTag != PreviousWeaponTag)
		{
			FCombatAttackRequest Request{};
			Request.NotifyType = ECombatNotifyType::WeaponChange;
			Request.WeaponId = FTimeThiefGameplayTags::ResolveWeaponIdFromTag(CurrentEquippedWeaponTag);
			BroadcastCombatAttackRequest(Request);
		}
		return;
	}

	if (InputTag == Tags.InputTag_Action_Fire)
	{
		bIsFireInputHeld = true;
		if (MasterWeaponPtr && !bIsEquippingWeapon)
		{
			MasterWeaponPtr->StartFire();
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
		MasterWeaponPtr->StartFire();
	}
}

void UTimeThiefPlayerCombatComponent::StartAiming()
{
	bIsAiming = true;

	FCombatAttackRequest Request{};
	Request.NotifyType = ECombatNotifyType::Aiming;
	BroadcastCombatAttackRequest(Request);
}

void UTimeThiefPlayerCombatComponent::StopAiming()
{
	if (!bIsAiming) return;

	bIsAiming = false;

	FCombatAttackRequest Request{};
	Request.NotifyType = ECombatNotifyType::Readying;
	BroadcastCombatAttackRequest(Request);
}


void UTimeThiefPlayerCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (const APawn* OwningPawn = GetPawn<APawn>(); OwningPawn && OwningPawn->IsLocallyControlled())
	{
		UpdateLocalWorldAimLocation();
		ApplyAimYawOverflowRotation(DeltaTime);
	}

	if (IsFiringWeapon())
	{
		if (UWorld* World = GetWorld())
		{
			LastFireTime = World->GetTimeSeconds();
		}
	}

	UpdateAimFOV(DeltaTime);
}

void UTimeThiefPlayerCombatComponent::UpdateLocalWorldAimLocation()
{
	APawn* OwningPawn = GetPawn<APawn>();
	if (!OwningPawn || !OwningPawn->IsLocallyControlled())
	{
		return;
	}

	FVector ViewLocation = FVector::ZeroVector;
	FVector ViewDirection = OwningPawn->GetActorForwardVector();
	if (!UTimeThiefAimStatics::ResolveAimView(OwningPawn, ViewLocation, ViewDirection))
	{
		CachedWorldAimLocation = UTimeThiefAimStatics::ResolveAimTargetLocation(
			GetEffectiveShotOrigin(),
			OwningPawn->GetActorForwardVector(),
			AimTraceRange);
		return;
	}

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Reserve(2);
	ActorsToIgnore.Add(OwningPawn);
	if (MasterWeaponPtr)
	{
		ActorsToIgnore.Add(MasterWeaponPtr);
	}

	FHitResult HitResult;
	FVector TraceEnd = FVector::ZeroVector;
	const bool bBlockingHit = UTimeThiefAimStatics::TraceFromView(
		GetWorld(),
		ViewLocation,
		ViewDirection,
		AimTraceRange,
		ActorsToIgnore,
		HitResult,
		TraceEnd);

	CachedWorldAimLocation = bBlockingHit ? HitResult.ImpactPoint : TraceEnd;
}

void UTimeThiefPlayerCombatComponent::ApplyAimYawOverflowRotation(float DeltaTime)
{
	ACharacter* OwningCharacter = GetPawn<ACharacter>();
	if (!OwningCharacter || !OwningCharacter->IsLocallyControlled())
	{
		return;
	}

	UCharacterMovementComponent* MovementComp = OwningCharacter->GetCharacterMovement();
	if (!MovementComp)
	{
		return;
	}

	if (OwningCharacter->bUseControllerRotationYaw || MovementComp->bUseControllerDesiredRotation)
	{
		return;
	}

	FRotator CurrentRotation = OwningCharacter->GetActorRotation();

	// 1. 이동 중일 때: 캐릭터의 하체를 즉시 카메라(컨트롤러) 방향으로 일치시킴
	const bool bIsMoving = !MovementComp->GetCurrentAcceleration().IsNearlyZero() || MovementComp->Velocity.SizeSquared2D() > 1.0f;
	if (bIsMoving)
	{
		if (AController* PC = OwningCharacter->GetController())
		{
			FRotator TargetRotation = CurrentRotation;
			TargetRotation.Yaw = PC->GetControlRotation().Yaw;
			
			// 이동 중일 땐 어색하지 않게 카메라 방향으로 빠르게 부드럽게 정렬 (스트레이핑 모드)
			FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, AimInterpSpeed * 1.5f);
			OwningCharacter->SetActorRotation(NewRotation);
		}
		return;
	}

	// 2. 정지 중일 때: 에임 오프셋 오버플로우 제한 로직 적용 (발을 땅에 고정시키고 상체만 돌리기 위함)
	if (!bRotateCharacterFromAimYawOverflow)
	{
		return;
	}

	if (CachedWorldAimLocation.IsNearlyZero())
	{
		return;
	}

	const FVector AimDirection = UTimeThiefAimStatics::ResolveAimDirectionToTarget(
		OwningCharacter->GetActorLocation(),
		CachedWorldAimLocation,
		OwningCharacter->GetActorForwardVector()
	);

	float AimWorldYaw = AimDirection.Rotation().Yaw;
	float DeltaYaw = FMath::FindDeltaAngleDegrees(CurrentRotation.Yaw, AimWorldYaw);

	const float Threshold = FMath::Clamp(AimYawOverflowTurnThreshold, 0.0f, 179.9f);
	float TargetYaw = CurrentRotation.Yaw;

	if (DeltaYaw > Threshold)
	{
		TargetYaw = AimWorldYaw - Threshold;
	}
	else if (DeltaYaw < -Threshold)
	{
		TargetYaw = AimWorldYaw + Threshold;
	}
	else
	{
		return;
	}

	FRotator TargetRotation = CurrentRotation;
	TargetRotation.Yaw = TargetYaw;

	FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, AimInterpSpeed);
	OwningCharacter->SetActorRotation(NewRotation);
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

void UTimeThiefPlayerCombatComponent::UpdateAimFOV(float DeltaTime)
{
	const float TargetFOV = bIsAiming ? AimFOV : DefaultFOV;

	for (UCameraComponent* Camera : { CachedThirdPersonCamera.Get(), CachedFirstPersonCamera.Get() })
	{
		if (!Camera)
		{
			continue;
		}

		if (FMath::IsNearlyEqual(Camera->FieldOfView, TargetFOV, 0.1f))
		{
			continue;
		}

		const float NewFOV = FMath::FInterpTo(Camera->FieldOfView, TargetFOV, DeltaTime, AimInterpSpeed);
		Camera->SetFieldOfView(NewFOV);
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
	MC->MaxWalkSpeed = DefaultMaxWalkSpeed;
}