#include "Animation/Player/TimeThiefPlayerAnimInstance.h"
#include "Character/TimeThiefPlayerCharacter.h"
#include "CharacterTrajectoryComponent.h"
#include "Components/Combat/TimeThiefPawnCombatComponent.h"
#include "Components/Wire/TimeThiefWireComponent.h"
#include "Weapon/TimeThiefWeaponBase.h"

UTimeThiefPlayerAnimInstance::UTimeThiefPlayerAnimInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer) {
	bHasWeapon = false;
	LeftHandIKTransform = FTransform::Identity;
	bIsWireAttached = false;
	AnchorDirection = FVector::ForwardVector;
	SwingVelocity = FVector::ZeroVector;
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
		PlayerCharacter = Cast<ATimeThiefPlayerCharacter>(TryGetPawnOwner());
	}

	if (!TrajectoryComponent && PlayerCharacter) {
		TrajectoryComponent = PlayerCharacter->GetCharacterTrajectoryComponent();
	}

	if (!WireComponent && PlayerCharacter) {
		WireComponent = PlayerCharacter->GetWireComponent();
	}

	UpdateWeaponData();
	UpdateWireData();
	UpdateRecoil(DeltaSeconds);
	UpdateSpreadAndAim(DeltaSeconds);
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

		USkeletalMeshComponent* WeaponMesh = CurrentWeapon->GetWeaponMesh();
		if (WeaponMesh && WeaponMesh->DoesSocketExist(LeftHandIKSocketName)) {
			FTransform SocketTransform = WeaponMesh->GetSocketTransform(LeftHandIKSocketName, RTS_World);
			
			USkeletalMeshComponent* OwningMesh = GetOwningComponent();
			if (OwningMesh) {
				FTransform MeshTransform = OwningMesh->GetComponentTransform();
				LeftHandIKTransform = SocketTransform.GetRelativeTransform(MeshTransform);
			}
		}
	} else {
		EquippedWeaponTag = FGameplayTag();
	}
}

void UTimeThiefPlayerAnimInstance::UpdateWireData() {
	if (!WireComponent) {
		bIsWireAttached = false;
		SwingVelocity = FVector::ZeroVector;
		return;
	}

	bIsWireAttached = WireComponent->IsWireAttached();

	if (bIsWireAttached) {
		FVector AnchorPoint = WireComponent->GetAnchorPoint();
		FVector StartLocation = WireComponent->GetWireStartLocation();
		AnchorDirection = (AnchorPoint - StartLocation).GetSafeNormal();
		
		if (USkeletalMeshComponent* Mesh = GetOwningComponent())
		{
			AnchorDirection = Mesh->GetComponentTransform().InverseTransformVectorNoScale(AnchorDirection);
		}
		
		if (PlayerCharacter) {
			SwingVelocity = PlayerCharacter->GetVelocity();
		}
	}
}

void UTimeThiefPlayerAnimInstance::TriggerRecoil(float Intensity)
{
	TargetRecoilAlpha = FMath::Clamp(Intensity, 0.0f, 1.0f);
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

void UTimeThiefPlayerAnimInstance::ApplyFireSpread()
{
	CurrentSpreadRatio = FMath::Clamp(CurrentSpreadRatio + SpreadIncreasePerShot, 0.0f, 1.0f);

	float HorizontalOffset = FMath::RandRange(-HorizontalRecoilRange, HorizontalRecoilRange);
	float VerticalOffset = VerticalRecoilAmount * (0.8f + FMath::RandRange(0.0f, 0.4f));

	TargetAimOffset.X += HorizontalOffset;
	TargetAimOffset.Y += VerticalOffset;

	TriggerRecoil(CurrentSpreadRatio);
}

void UTimeThiefPlayerAnimInstance::UpdateSpreadAndAim(float DeltaSeconds)
{
	CurrentSpreadRatio = FMath::FInterpTo(CurrentSpreadRatio, 0.0f, DeltaSeconds, SpreadRecoverySpeed);

	AimOffset = FMath::Vector2DInterpTo(AimOffset, TargetAimOffset, DeltaSeconds, 15.0f);
	TargetAimOffset = FMath::Vector2DInterpTo(TargetAimOffset, FVector2D::ZeroVector, DeltaSeconds, AimRecoverySpeed);
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

