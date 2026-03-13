#include "Animation/Player/TimeThiefPlayerAnimInstance.h"
#include "Character/TimeThiefPlayerCharacter.h"
#include "CharacterTrajectoryComponent.h"
#include "Components/Combat/TimeThiefPawnCombatComponent.h"
#include "Components/Combat/TimeThiefPlayerCombatComponent.h"
#include "Components/Wire/TimeThiefWireComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Weapon/TimeThiefWeaponBase.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"

UTimeThiefPlayerAnimInstance::UTimeThiefPlayerAnimInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer) {
	bHasWeapon = false;
	LeftHandIKTransform = FTransform::Identity;
	bIsWireAttached = false;
	bIsWireActive = false;
	AnchorDirection = FVector::ForwardVector;
	SwingVelocity = FVector::ZeroVector;
	WireLeftHandIKTransform = FTransform::Identity;
	WireLeftHandIKAlpha = 0.0f;
	WireAnchorDirectionWorld = FVector::ForwardVector;
	AimControlRigReleaseTimer = 0.0f;
}

void UTimeThiefPlayerAnimInstance::NativeInitializeAnimation() {
	Super::NativeInitializeAnimation();

	PlayerCharacter = Cast<ATimeThiefPlayerCharacter>(TryGetPawnOwner());
	if (PlayerCharacter) {
		TrajectoryComponent = PlayerCharacter->GetCharacterTrajectoryComponent();
		WireComponent = PlayerCharacter->GetWireComponent();
	}
}

void UTimeThiefPlayerAnimInstance::NativeUpdateAnimation(float DeltaSeconds) {
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!PlayerCharacter) {
		return;
	}

	UpdateWeaponData();
	UpdateAimingState(DeltaSeconds);
	UpdateWireData();
	UpdateWireHandIK(DeltaSeconds);
	UpdateRecoil(DeltaSeconds);
	UpdateSpreadAndRecoil(DeltaSeconds);
	UpdateAimDirection();
}

void UTimeThiefPlayerAnimInstance::UpdateWeaponData() {
	if (!PlayerCharacter) {
		bHasWeapon = false;
		CurrentWeapon = nullptr;
		EquippedWeaponTag = FGameplayTag();
		return;
	}

	UTimeThiefPawnCombatComponent* CombatComp = PlayerCharacter->GetPawnCombatComponent();
	if (!CombatComp) {
		bHasWeapon = false;
		CurrentWeapon = nullptr;
		EquippedWeaponTag = FGameplayTag();
		return;
	}

	CurrentWeapon = CombatComp->GetCharacterCurrentEquippedWeapon();
	bHasWeapon = (CurrentWeapon != nullptr);

	if (bHasWeapon) {
		EquippedWeaponTag = CurrentWeapon->GetWeaponTag();

		const FName LHIKSocket = CurrentWeapon->GetLeftHandIKSocketName();
		UStaticMeshComponent* WeaponMesh = CurrentWeapon->GetWeaponMesh();
		USkeletalMeshComponent* OwningMesh = GetOwningComponent();
		if (WeaponMesh && OwningMesh && WeaponMesh->DoesSocketExist(LHIKSocket))
		{
			FTransform SocketTransform = WeaponMesh->GetSocketTransform(LHIKSocket, RTS_World);
			LeftHandIKTransform = SocketTransform.GetRelativeTransform(OwningMesh->GetComponentTransform());
		}
	} else {
		EquippedWeaponTag = FGameplayTag();
	}
}

void UTimeThiefPlayerAnimInstance::UpdateWireData() {
	if (!WireComponent) {
		bIsWireAttached = false;
		bIsWireActive = false;
		SwingVelocity = FVector::ZeroVector;
		WireAnchorDirectionWorld = FVector::ForwardVector;
		return;
	}

	bIsWireAttached = WireComponent->IsWireAttached();
	bIsWireActive = WireComponent->IsWireActive();

	if (bIsWireActive) {
		FVector AnchorPoint = WireComponent->GetAnchorPoint();
		FVector StartLocation = WireComponent->GetWireStartLocation();
		WireAnchorDirectionWorld = (AnchorPoint - StartLocation).GetSafeNormal();
		AnchorDirection = WireAnchorDirectionWorld;
		
		if (USkeletalMeshComponent* Mesh = GetOwningComponent())
		{
			AnchorDirection = Mesh->GetComponentTransform().InverseTransformVectorNoScale(AnchorDirection);
		}
		
		if (PlayerCharacter) {
			SwingVelocity = PlayerCharacter->GetVelocity();
		}
	} else {
		WireAnchorDirectionWorld = FVector::ForwardVector;
	}
}

void UTimeThiefPlayerAnimInstance::TriggerRecoil(float Intensity)
{
	TargetRecoilAlpha = FMath::Clamp(Intensity, 0.0f, 1.0f);
}

void UTimeThiefPlayerAnimInstance::UpdateWireHandIK(float DeltaSeconds)
{
	const float TargetAlpha = bIsWireActive ? 1.0f : 0.0f;
	WireLeftHandIKAlpha = FMath::FInterpTo(WireLeftHandIKAlpha, TargetAlpha, DeltaSeconds, WireHandIKInterpSpeed);

	if (WireLeftHandIKAlpha < KINDA_SMALL_NUMBER)
	{
		WireLeftHandIKTransform = FTransform::Identity;
		return;
	}

	USkeletalMeshComponent* OwningMesh = GetOwningComponent();
	if (!OwningMesh || !WireComponent)
	{
		return;
	}

	if (OwningMesh->GetBoneIndex(WireHandBoneName) == INDEX_NONE)
	{
		return;
	}

	const FTransform MeshWorldTransform = OwningMesh->GetComponentTransform();
	const FVector WireStartWorld = WireComponent->GetWireStartLocation();
	const FVector TargetWorldLocation = WireStartWorld + WireAnchorDirectionWorld * WireHandReachDistance;
	const FVector TargetCS = MeshWorldTransform.InverseTransformPosition(TargetWorldLocation);

	const FVector AnchorDirCS = MeshWorldTransform.InverseTransformVectorNoScale(WireAnchorDirectionWorld);
	const FQuat LookAtRotation = AnchorDirCS.ToOrientationQuat();

	WireLeftHandIKTransform = FTransform(LookAtRotation, TargetCS, FVector::OneVector);
}

void UTimeThiefPlayerAnimInstance::UpdateRecoil(float DeltaSeconds)
{
	if (TargetRecoilAlpha > 0.0f)
	{
		RecoilAlpha = FMath::FInterpTo(RecoilAlpha, TargetRecoilAlpha, DeltaSeconds, RecoilInterpSpeed * 2.0f);
		TargetRecoilAlpha = 0.0f;
	}
	else
	{
		RecoilAlpha = FMath::FInterpTo(RecoilAlpha, 0.0f, DeltaSeconds, RecoilInterpSpeed);
	}
}

FVector2D UTimeThiefPlayerAnimInstance::ApplyFireSpread(float InMaxVerticalRecoil, float InMaxHorizontalRecoil, float InRecoilBuildupPerShot, float InSpreadBuildupPerShot)
{
	RecoilBuildup = FMath::Clamp(RecoilBuildup + InRecoilBuildupPerShot, 0.0f, 1.0f);
	CurrentSpreadRatio = FMath::Clamp(CurrentSpreadRatio + InSpreadBuildupPerShot, 0.0f, 1.0f);

	const float VerticalRecoil = InMaxVerticalRecoil * RecoilBuildup * FMath::FRandRange(0.7f, 1.0f);
	const float HorizontalRecoil = FMath::FRandRange(-InMaxHorizontalRecoil, InMaxHorizontalRecoil) * RecoilBuildup;

	TargetAimOffset.X += HorizontalRecoil;
	TargetAimOffset.Y += VerticalRecoil;

	TriggerRecoil(RecoilBuildup);

	return FVector2D(HorizontalRecoil, VerticalRecoil);
}

void UTimeThiefPlayerAnimInstance::UpdateSpreadAndRecoil(float DeltaSeconds)
{
	CurrentSpreadRatio = FMath::FInterpTo(CurrentSpreadRatio, 0.0f, DeltaSeconds, SpreadRecoverySpeed);
	RecoilBuildup = FMath::FInterpTo(RecoilBuildup, 0.0f, DeltaSeconds, RecoilRecoverySpeed);
	AimOffset = FMath::Vector2DInterpTo(AimOffset, TargetAimOffset, DeltaSeconds, AimOffsetInterpSpeed);
	TargetAimOffset = FMath::Vector2DInterpTo(TargetAimOffset, FVector2D::ZeroVector, DeltaSeconds, RecoilRecoverySpeed);
}

void UTimeThiefPlayerAnimInstance::UpdateAimDirection()
{
	if (!PlayerCharacter)
	{
		AimPitch = 0.0f;
		AimDirection = FVector::ForwardVector;
		WorldAimLocation = FVector::ZeroVector;
		return;
	}

	if (!bShouldApplyAimControlRig)
	{
		const FVector CharacterForward = PlayerCharacter->GetActorForwardVector();
		AimPitch = 0.0f;
		AimDirection = CharacterForward;
		WorldAimLocation = PlayerCharacter->GetActorLocation() + (CharacterForward * 1000.0f);
		return;
	}

	FVector CameraLocation = FVector::ZeroVector;
	FRotator CameraRotation = PlayerCharacter->GetControlRotation();
	
	if (APlayerController* PC = Cast<APlayerController>(PlayerCharacter->GetController()))
	{
		PC->GetPlayerViewPoint(CameraLocation, CameraRotation);
	}

	AimDirection = CameraRotation.Vector();
	WorldAimLocation = CameraLocation + (AimDirection * 10000.0f);

	const float HorizontalSize = FVector2D(AimDirection.X, AimDirection.Y).Size();
	AimPitch = -FMath::RadiansToDegrees(FMath::Atan2(AimDirection.Z, HorizontalSize));
}

void UTimeThiefPlayerAnimInstance::UpdateAimingState(float DeltaSeconds)
{
	if (!PlayerCharacter)
	{
		bIsAiming = false;
		bIsFiringWeapon = false;
		bShouldApplyAimControlRig = false;
		AimControlRigReleaseTimer = 0.0f;
		AimSpreadMultiplier = 1.0f;
		return;
	}

	if (UTimeThiefPlayerCombatComponent* PlayerCombat = PlayerCharacter->GetPlayerCombatComponent())
	{
		bIsAiming = PlayerCombat->IsAiming();
		bIsFiringWeapon = PlayerCombat->IsFiringWeapon();

		const bool bWantsAimControlRig = bIsAiming || bIsFiringWeapon;
		if (bWantsAimControlRig)
		{
			AimControlRigReleaseTimer = AimControlRigReleaseHoldTime;
			bShouldApplyAimControlRig = true;
		}
		else
		{
			AimControlRigReleaseTimer = FMath::Max(0.0f, AimControlRigReleaseTimer - DeltaSeconds);
			bShouldApplyAimControlRig = AimControlRigReleaseTimer > 0.0f;
		}

		AimSpreadMultiplier = bIsAiming ? PlayerCombat->GetAimSpreadMultiplier() : 1.0f;
	}
	else
	{
		bIsAiming = false;
		bIsFiringWeapon = false;
		bShouldApplyAimControlRig = false;
		AimControlRigReleaseTimer = 0.0f;
		AimSpreadMultiplier = 1.0f;
	}
}