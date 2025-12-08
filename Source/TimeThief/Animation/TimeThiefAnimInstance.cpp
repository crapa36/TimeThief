#include "TimeThiefAnimInstance.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Logging/StructuredLog.h"

#include "TimeThief/Components/Combat/TimeThiefPawnCombatComponent.h"
#include "TimeThief/Weapon/TimeThiefWeaponBase.h"

UTimeThiefAnimInstance::UTimeThiefAnimInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer) {
}

void UTimeThiefAnimInstance::NativeInitializeAnimation() {
	Super::NativeInitializeAnimation();

	// Init Log
	UE_LOGFMT(LogAnimation, Warning, "TimeThiefAnimInstance: NativeInitializeAnimation Called on {0}", GetName());

	CharacterOwner = Cast<ACharacter>(TryGetPawnOwner());
	UpdateCombatComponent();
}

void UTimeThiefAnimInstance::NativeUpdateAnimation(float DeltaSeconds) {
	Super::NativeUpdateAnimation(DeltaSeconds);

	// 1. Check if Function is running (Log every 3 seconds)
	DebugLogTimer += DeltaSeconds;
	if (DebugLogTimer > 3.0f) {
		DebugLogTimer = 0.0f;
		if (!IsValid(CharacterOwner)) {
			UE_LOGFMT(LogAnimation, Error, "TimeThiefAnimInstance: CharacterOwner is NULL. Reparent Check OK?");
		}
		else if (!IsValid(CombatComponent)) {
			UE_LOGFMT(LogAnimation, Error, "TimeThiefAnimInstance: Character Found ({0}), but CombatComponent MISSING.", GetNameSafe(CharacterOwner));
		}
		else {
			// Normal operation ping
			// UE_LOGFMT(LogAnimation, Log, "TimeThiefAnimInstance: Running OK. Current Weapon: {0}", GetNameSafe(CurrentWeapon));
		}
	}

	if (!IsValid(CharacterOwner)) {
		CharacterOwner = Cast<ACharacter>(TryGetPawnOwner());
	}

	if (IsValid(CharacterOwner) && !IsValid(CombatComponent)) {
		UpdateCombatComponent();
	}

	ProcessWeaponLayerUpdate();
}

void UTimeThiefAnimInstance::UpdateCombatComponent() {
	if (!IsValid(CharacterOwner)) {
		return;
	}

	// Try finding component
	CombatComponent = CharacterOwner->FindComponentByClass<UTimeThiefPawnCombatComponent>();

	if (IsValid(CombatComponent)) {
		UE_LOGFMT(LogAnimation, Display, "TimeThiefAnimInstance: CombatComponent Successfully Found on {0}", GetNameSafe(CharacterOwner));
	}
}

void UTimeThiefAnimInstance::ProcessWeaponLayerUpdate() {
	if (!IsValid(CombatComponent)) {
		return;
	}

	ATimeThiefWeaponBase* NewWeapon = CombatComponent->GetCharacterCurrentEquippedWeapon();

	// Force log if weapon state changes
	if (CurrentWeapon != NewWeapon) {
		UE_LOGFMT(LogAnimation, Warning, "TimeThiefAnimInstance: Weapon Switch Detected! Old: {0} -> New: {1}", GetNameSafe(CurrentWeapon), GetNameSafe(NewWeapon));

		CurrentWeapon = NewWeapon;

		TSubclassOf<UAnimInstance> NewLayerClass = nullptr;
		if (IsValid(CurrentWeapon)) {
			NewLayerClass = CurrentWeapon->GetEquipAnimLayer();
			if (!NewLayerClass) {
				UE_LOGFMT(LogAnimation, Error, "TimeThiefAnimInstance: Weapon {0} has NO AnimLayer set!", GetNameSafe(CurrentWeapon));
			}
		}

		if (CurrentLinkedLayerClass != NewLayerClass) {
			if (CurrentLinkedLayerClass) {
				UnlinkAnimClassLayers(CurrentLinkedLayerClass);
				UE_LOGFMT(LogAnimation, Display, "TimeThiefAnimInstance: Unlinked {0}", GetNameSafe(CurrentLinkedLayerClass));
			}

			if (NewLayerClass) {
				LinkAnimClassLayers(NewLayerClass);
				UE_LOGFMT(LogAnimation, Display, "TimeThiefAnimInstance: LINKED {0}", GetNameSafe(NewLayerClass));
			}

			CurrentLinkedLayerClass = NewLayerClass;
		}
		else {
			UE_LOGFMT(LogAnimation, Warning, "TimeThiefAnimInstance: Layer Class did not change (Both are {0}). Check BP_Rifle settings.", GetNameSafe(NewLayerClass));
		}
	}
}