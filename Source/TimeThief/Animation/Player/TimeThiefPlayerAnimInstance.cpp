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
