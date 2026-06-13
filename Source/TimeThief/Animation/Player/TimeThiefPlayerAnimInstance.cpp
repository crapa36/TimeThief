#include "Animation/Player/TimeThiefPlayerAnimInstance.h"
#include "Character/TimeThiefSkillDummyCharacter.h"
#include "Character/TimeThiefPlayerCharacter.h"
#include "Components/Combat/TimeThiefPawnCombatComponent.h"
#include "Components/Combat/TimeThiefPlayerCombatComponent.h"
#include "Components/Wire/TimeThiefWireComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Weapon/Components/TimeThiefWeaponComponentBase.h"
#include "Weapon/TimeThiefMasterWeapon.h"
#include "Network/MovableNetworkEntityInterface.h"
#include "Engine/Engine.h"
#include "Utils/TimeThiefAimStatics.h"

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

	APawn* PawnOwner = TryGetPawnOwner();
	if (!PlayerCharacter || PlayerCharacter != PawnOwner) {
		PlayerCharacter = Cast<ATimeThiefPlayerCharacter>(PawnOwner);
		if (PlayerCharacter) {
			WireComponent = PlayerCharacter->GetWireComponent();
		} else {
			WireComponent = nullptr;
		}
	}

	if (!PlayerCharacter) {
		if (ATimeThiefSkillDummyCharacter* SkillDummy = Cast<ATimeThiefSkillDummyCharacter>(PawnOwner)) {
			UpdateDummyAimData(SkillDummy);
			UpdateDummyWeaponData(SkillDummy);
			bIsWireAttached = false;
			bIsWireActive = false;
			SwingVelocity = FVector::ZeroVector;
			WireAnchorDirectionWorld = FVector::ForwardVector;
			WireLeftHandIKAlpha = 0.0f;
			WireLeftHandIKTransform = FTransform::Identity;
			UpdateRecoil(DeltaSeconds);
			UpdateSpreadAndRecoil(DeltaSeconds);
		}
		return;
	}

	UpdateAimData();
	UpdateWeaponData();
	UpdateWireData();
	UpdateWireHandIK(DeltaSeconds);
	UpdateRecoil(DeltaSeconds);
	UpdateSpreadAndRecoil(DeltaSeconds);
}

void UTimeThiefPlayerAnimInstance::UpdateAimData() {
	if (!PlayerCharacter) {
		AimPitch = 0.0f;
		AimYaw = 0.0f;
		AimDirection = FVector::ForwardVector;
		WorldAimLocation = FVector::ZeroVector;
		ControlRigWorldAimLocation = FVector::ZeroVector;
		ControlRigAimLocationCS = FVector::ZeroVector;
		bHasValidControlRigAimLocation = false;
		return;
	}

	if (PlayerCharacter->IsLocallyControlled()) {
		FVector ViewLocation = PlayerCharacter->GetPawnViewLocation();
		FVector ViewDirection = UTimeThiefAimStatics::NormalizeAimDirection(PlayerCharacter->GetActorForwardVector());
		if (!UTimeThiefAimStatics::ResolveAimView(PlayerCharacter, ViewLocation, ViewDirection) || ViewDirection.IsNearlyZero()) {
			ViewDirection = UTimeThiefAimStatics::NormalizeAimDirection(PlayerCharacter->GetActorForwardVector());
		}

		WorldAimLocation = UTimeThiefAimStatics::ResolveAimTargetLocation(ViewLocation, ViewDirection, 10000.0f);
		if (const UTimeThiefPlayerCombatComponent* CombatComp = PlayerCharacter->GetPlayerCombatComponent()) {
			const FVector CombatAimLocation = CombatComp->GetWorldAimLocation();
			if (!CombatAimLocation.IsNearlyZero()) {
				WorldAimLocation = CombatAimLocation;
			}
		}

		ControlRigWorldAimLocation = WorldAimLocation;
		bHasValidControlRigAimLocation = !ControlRigWorldAimLocation.IsNearlyZero();
		if (const USkeletalMeshComponent* OwningMesh = GetOwningComponent()) {
			ControlRigAimLocationCS = OwningMesh->GetComponentTransform().InverseTransformPosition(ControlRigWorldAimLocation);
		} else {
			ControlRigAimLocationCS = FVector::ZeroVector;
		}

		const FVector CharacterLocation = PlayerCharacter->GetActorLocation();
		const FVector AimLine = WorldAimLocation - CharacterLocation;
		const FVector ToAimFromCharacter = UTimeThiefAimStatics::ResolveAimDirectionToTarget(
			CharacterLocation,
			CharacterLocation + AimLine,
			ViewDirection);

		AimDirection = ToAimFromCharacter;

		UTimeThiefAimStatics::ResolveRelativeAimPitchYaw(
			PlayerCharacter->GetActorTransform(),
			ToAimFromCharacter,
			AimPitch,
			AimYaw,
			ViewDirection);
	} else if (IMovableNetworkEntityInterface* Movable = Cast<IMovableNetworkEntityInterface>(PlayerCharacter)) {
		AimYaw = Movable->GetNetworkAimYaw();
		AimPitch = Movable->GetNetworkAimPitch();

		const FRotator RelativeAimRot(AimPitch, AimYaw, 0.0f);
		AimDirection = PlayerCharacter->GetActorTransform().TransformVectorNoScale(RelativeAimRot.Vector());
		WorldAimLocation = PlayerCharacter->GetPawnViewLocation() + (AimDirection * 10000.0f);

		ControlRigWorldAimLocation = WorldAimLocation;
		bHasValidControlRigAimLocation = !ControlRigWorldAimLocation.IsNearlyZero();
		if (const USkeletalMeshComponent* OwningMesh = GetOwningComponent()) {
			ControlRigAimLocationCS = OwningMesh->GetComponentTransform().InverseTransformPosition(ControlRigWorldAimLocation);
		} else {
			ControlRigAimLocationCS = FVector::ZeroVector;
		}
	}
}

void UTimeThiefPlayerAnimInstance::UpdateDummyAimData(ATimeThiefSkillDummyCharacter* SkillDummy) {
	if (!SkillDummy) {
		AimPitch = 0.0f;
		AimYaw = 0.0f;
		AimDirection = FVector::ForwardVector;
		WorldAimLocation = FVector::ZeroVector;
		ControlRigWorldAimLocation = FVector::ZeroVector;
		ControlRigAimLocationCS = FVector::ZeroVector;
		bHasValidControlRigAimLocation = false;
		return;
	}

	const FVector ViewDirection = UTimeThiefAimStatics::NormalizeAimDirection(SkillDummy->GetActorForwardVector());
	const FVector ViewLocation = SkillDummy->GetPawnViewLocation();

	AimDirection = ViewDirection;
	WorldAimLocation = UTimeThiefAimStatics::ResolveAimTargetLocation(ViewLocation, ViewDirection, 10000.0f);
	ControlRigWorldAimLocation = WorldAimLocation;
	bHasValidControlRigAimLocation = !ControlRigWorldAimLocation.IsNearlyZero();

	if (const USkeletalMeshComponent* OwningMesh = GetOwningComponent()) {
		ControlRigAimLocationCS = OwningMesh->GetComponentTransform().InverseTransformPosition(ControlRigWorldAimLocation);
	} else {
		ControlRigAimLocationCS = FVector::ZeroVector;
	}

	UTimeThiefAimStatics::ResolveRelativeAimPitchYaw(
		SkillDummy->GetActorTransform(),
		ViewDirection,
		AimPitch,
		AimYaw,
		ViewDirection);
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

void UTimeThiefPlayerAnimInstance::UpdateDummyWeaponData(ATimeThiefSkillDummyCharacter* SkillDummy) {
	CurrentWeapon = SkillDummy ? SkillDummy->GetCopiedWeaponComponent() : nullptr;
	EquippedWeaponTag = SkillDummy ? SkillDummy->GetCopiedWeaponTag() : FGameplayTag();
	LeftHandIKTransform = FTransform::Identity;

	UStaticMeshComponent* WeaponMesh = SkillDummy ? SkillDummy->GetCopiedWeaponMesh() : nullptr;
	bHasWeapon = WeaponMesh && WeaponMesh->GetStaticMesh();
	if (!bHasWeapon) {
		return;
	}

	const FName LHIKSocket = SkillDummy->GetCopiedWeaponLeftHandIKSocketName();
	USkeletalMeshComponent* OwningMesh = GetOwningComponent();
	if (OwningMesh && WeaponMesh->DoesSocketExist(LHIKSocket)) {
		const FTransform SocketTransform = WeaponMesh->GetSocketTransform(LHIKSocket, RTS_World);
		LeftHandIKTransform = SocketTransform.GetRelativeTransform(OwningMesh->GetComponentTransform());
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
}
