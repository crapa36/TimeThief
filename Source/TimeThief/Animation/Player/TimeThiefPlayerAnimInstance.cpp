#include "Animation/Player/TimeThiefPlayerAnimInstance.h"
#include "Character/TimeThiefPlayerCharacter.h"
#include "CharacterTrajectoryComponent.h"
#include "Components/Combat/TimeThiefPawnCombatComponent.h"
#include "Components/Combat/TimeThiefPlayerCombatComponent.h"
#include "Components/Wire/TimeThiefWireComponent.h"
#include "Weapon/TimeThiefWeaponBase.h"

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
	UpdateAimingState();
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

void UTimeThiefPlayerAnimInstance::ApplyFireSpread(float InMaxVerticalRecoil, float InMaxHorizontalRecoil, float InRecoilBuildupPerShot, float InSpreadBuildupPerShot)
{
	RecoilBuildup = FMath::Clamp(RecoilBuildup + InRecoilBuildupPerShot, 0.0f, 1.0f);
	CurrentSpreadRatio = FMath::Clamp(CurrentSpreadRatio + InSpreadBuildupPerShot, 0.0f, 1.0f);

	const float VerticalRecoil = InMaxVerticalRecoil * RecoilBuildup * FMath::FRandRange(0.7f, 1.0f);
	const float HorizontalRecoil = FMath::FRandRange(-InMaxHorizontalRecoil, InMaxHorizontalRecoil) * RecoilBuildup;

	TargetAimOffset.X += HorizontalRecoil;
	TargetAimOffset.Y += VerticalRecoil;

	TriggerRecoil(RecoilBuildup);
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
		AimYaw = 0.0f;
		return;
	}

	FRotator ControlRotation = PlayerCharacter->GetControlRotation();
	FRotator ActorRotation = PlayerCharacter->GetActorRotation();

	FRotator DeltaRotation = (ControlRotation - ActorRotation).GetNormalized();

	AimPitch = FMath::Clamp(DeltaRotation.Pitch, -90.0f, 90.0f);
	AimYaw = FMath::Clamp(DeltaRotation.Yaw, -90.0f, 90.0f);
}

void UTimeThiefPlayerAnimInstance::UpdateAimingState()
{
	if (!PlayerCharacter)
	{
		bIsAiming = false;
		AimSpreadMultiplier = 1.0f;
		return;
	}

	if (UTimeThiefPlayerCombatComponent* PlayerCombat = PlayerCharacter->GetPlayerCombatComponent())
	{
		bIsAiming = PlayerCombat->IsAiming();
		AimSpreadMultiplier = bIsAiming ? PlayerCombat->GetAimSpreadMultiplier() : 1.0f;
	}
	else
	{
		bIsAiming = false;
		AimSpreadMultiplier = 1.0f;
	}
}
