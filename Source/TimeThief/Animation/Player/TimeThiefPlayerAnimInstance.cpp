#include "Animation/Player/TimeThiefPlayerAnimInstance.h"
#include "Character/TimeThiefPlayerCharacter.h"
#include "Components/Combat/TimeThiefPawnCombatComponent.h"
#include "Components/Combat/TimeThiefPlayerCombatComponent.h"
#include "Components/Wire/TimeThiefWireComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Weapon/Components/TimeThiefWeaponComponentBase.h"
#include "Weapon/TimeThiefMasterWeapon.h"
#include "Network/MovableNetworkEntityInterface.h"
#include "Engine/Engine.h"

UTimeThiefPlayerAnimInstance::UTimeThiefPlayerAnimInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer) {
}

void UTimeThiefPlayerAnimInstance::NativeInitializeAnimation() {
	Super::NativeInitializeAnimation();

	PlayerCharacter = Cast<ATimeThiefPlayerCharacter>(TryGetPawnOwner());
	if (PlayerCharacter) {
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
	UTimeThiefPawnCombatComponent* CombatComp = PlayerCharacter->GetCombatComponent();
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
		if (ATimeThiefMasterWeapon* MasterWeapon = Cast<ATimeThiefMasterWeapon>(CurrentWeapon->GetOwner())) {
			UStaticMeshComponent* WeaponMesh = MasterWeapon->GetWeaponMesh();
			USkeletalMeshComponent* OwningMesh = GetOwningComponent();
			if (WeaponMesh && OwningMesh && WeaponMesh->DoesSocketExist(LHIKSocket)) {
				FTransform SocketTransform = WeaponMesh->GetSocketTransform(LHIKSocket, RTS_World);
				LeftHandIKTransform = SocketTransform.GetRelativeTransform(OwningMesh->GetComponentTransform());
			}
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

		if (USkeletalMeshComponent* Mesh = GetOwningComponent()) {
			AnchorDirection = Mesh->GetComponentTransform().InverseTransformVectorNoScale(AnchorDirection);
		}

		SwingVelocity = Velocity;
	} else {
		WireAnchorDirectionWorld = FVector::ForwardVector;
	}
}

void UTimeThiefPlayerAnimInstance::TriggerRecoil(float Intensity) {
	TargetRecoilAlpha = FMath::Clamp(Intensity, 0.0f, 1.0f);
}

void UTimeThiefPlayerAnimInstance::SetRecoilRecoverySpeed(float InRecoilRecovery, float InSpreadRecovery) {
	RecoilRecoverySpeed = InRecoilRecovery;
	SpreadRecoverySpeed = InSpreadRecovery;
}

void UTimeThiefPlayerAnimInstance::UpdateWireHandIK(float DeltaSeconds) {
	const float TargetAlpha = bIsWireActive ? 1.0f : 0.0f;
	WireLeftHandIKAlpha = FMath::FInterpTo(WireLeftHandIKAlpha, TargetAlpha, DeltaSeconds, WireHandIKInterpSpeed);

	if (WireLeftHandIKAlpha < KINDA_SMALL_NUMBER) {
		WireLeftHandIKTransform = FTransform::Identity;
		return;
	}

	USkeletalMeshComponent* OwningMesh = GetOwningComponent();
	if (!OwningMesh || !WireComponent) {
		return;
	}

	if (OwningMesh->GetBoneIndex(WireHandBoneName) == INDEX_NONE) {
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

void UTimeThiefPlayerAnimInstance::UpdateRecoil(float DeltaSeconds) {
	if (TargetRecoilAlpha > 0.0f) {
		RecoilAlpha = FMath::FInterpTo(RecoilAlpha, TargetRecoilAlpha, DeltaSeconds, RecoilInterpSpeed * 2.0f);
		TargetRecoilAlpha = 0.0f;
	} else {
		RecoilAlpha = FMath::FInterpTo(RecoilAlpha, 0.0f, DeltaSeconds, RecoilInterpSpeed);
	}
}

FVector2D UTimeThiefPlayerAnimInstance::ApplyFireSpread(float InMaxVerticalRecoil, float InMaxHorizontalRecoil, float InRecoilBuildupPerShot, float InSpreadBuildupPerShot) {
	const float VerticalRecoil = FMath::Lerp(InMaxVerticalRecoil * 0.2f, InMaxVerticalRecoil, RecoilBuildup) * FMath::FRandRange(0.85f, 1.15f);
	const float HorizontalRecoil = FMath::FRandRange(-InMaxHorizontalRecoil, InMaxHorizontalRecoil) * FMath::Lerp(0.3f, 1.0f, RecoilBuildup);

	RecoilBuildup = FMath::Clamp(RecoilBuildup + InRecoilBuildupPerShot, 0.0f, 1.0f);
	CurrentSpreadRatio = FMath::Clamp(CurrentSpreadRatio + InSpreadBuildupPerShot, 0.0f, 1.0f);

	TargetAimOffset.X += HorizontalRecoil;
	TargetAimOffset.Y += VerticalRecoil;

	TriggerRecoil(RecoilBuildup);

	return FVector2D(HorizontalRecoil, VerticalRecoil);
}

void UTimeThiefPlayerAnimInstance::UpdateSpreadAndRecoil(float DeltaSeconds) {
	CurrentSpreadRatio = FMath::FInterpTo(CurrentSpreadRatio, 0.0f, DeltaSeconds, SpreadRecoverySpeed);
	RecoilBuildup = FMath::FInterpTo(RecoilBuildup, 0.0f, DeltaSeconds, RecoilRecoverySpeed);
	AimOffset = FMath::Vector2DInterpTo(AimOffset, TargetAimOffset, DeltaSeconds, AimOffsetInterpSpeed);
	TargetAimOffset = FMath::Vector2DInterpTo(TargetAimOffset, FVector2D::ZeroVector, DeltaSeconds, RecoilRecoverySpeed);
}

void UTimeThiefPlayerAnimInstance::UpdateAimDirection() {
	FRotator BaseAimRotation = PlayerCharacter->GetBaseAimRotation();

	if (MovableNetworkInterface.GetInterface() && !PlayerCharacter->IsLocallyControlled()) {
		BaseAimRotation = FRotator(MovableNetworkInterface->GetNetworkPitch(), MovableNetworkInterface->GetNetworkYaw(), 0.0f);
	}

	const FRotator ActorRotation = PlayerCharacter->GetActorRotation();
	const FRotator DeltaRotation = (BaseAimRotation - ActorRotation).GetNormalized();

	AimPitch = DeltaRotation.Pitch;
	AimYaw = DeltaRotation.Yaw;

	if (bIsAiming || bUseWeaponControlRigRotation) {
		AimDirection = BaseAimRotation.Vector().GetSafeNormal();
	} else {
		AimDirection = BaseAimRotation.Vector().GetSafeNormal2D();
	}

	if (UTimeThiefPlayerCombatComponent* PlayerCombat = PlayerCharacter->GetPlayerCombatComponent()) {
		WorldAimLocation = PlayerCombat->GetWorldAimLocation();
	} else {
		FVector StartLoc = PlayerCharacter->GetActorLocation();
		if (CurrentWeapon && CurrentWeapon->GetOwner()) {
			StartLoc = CurrentWeapon->GetOwner()->GetActorLocation();
		}
		WorldAimLocation = StartLoc + (AimDirection * 50000.0f);
	}
}

void UTimeThiefPlayerAnimInstance::UpdateAimingState() {
	if (UTimeThiefPlayerCombatComponent* PlayerCombat = PlayerCharacter->GetPlayerCombatComponent()) {
		bIsAiming = PlayerCombat->IsAiming();
		AimSpreadMultiplier = bIsAiming ? PlayerCombat->GetAimSpreadMultiplier() : 1.0f;
	} else {
		bIsAiming = false;
		AimSpreadMultiplier = 1.0f;
		bUseWeaponControlRigRotation = false;
	}
}