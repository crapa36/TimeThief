#include "Animation/Player/TimeThiefFirstPersonAnimInstance.h"
#include "Character/TimeThiefPlayerCharacter.h"
#include "Components/Combat/TimeThiefPawnCombatComponent.h"
#include "Weapon/TimeThiefWeaponBase.h"
#include "GameFramework/Controller.h"

UTimeThiefFirstPersonAnimInstance::UTimeThiefFirstPersonAnimInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bHasWeapon = false;
	SwayRotation = FRotator::ZeroRotator;
	SwayLocation = FVector::ZeroVector;
}

void UTimeThiefFirstPersonAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	PlayerCharacter = Cast<ATimeThiefPlayerCharacter>(TryGetPawnOwner());
	
	if (PlayerCharacter)
	{
		LastRotation = PlayerCharacter->GetControlRotation();
	}
}

void UTimeThiefFirstPersonAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!PlayerCharacter)
	{
		PlayerCharacter = Cast<ATimeThiefPlayerCharacter>(TryGetPawnOwner());
	}

	UpdateWeaponData();
	UpdateSway(DeltaSeconds);
}

void UTimeThiefFirstPersonAnimInstance::UpdateWeaponData()
{
	if (!PlayerCharacter)
	{
		bHasWeapon = false;
		CurrentWeapon = nullptr;
		EquippedWeaponTag = FGameplayTag();
		return;
	}

	UTimeThiefPawnCombatComponent* CombatComp = PlayerCharacter->GetPawnCombatComponent();
	if (!CombatComp)
	{
		bHasWeapon = false;
		CurrentWeapon = nullptr;
		EquippedWeaponTag = FGameplayTag();
		return;
	}

	CurrentWeapon = CombatComp->GetCharacterCurrentEquippedWeapon();
	bHasWeapon = (CurrentWeapon != nullptr);

	if (bHasWeapon)
	{
		EquippedWeaponTag = CurrentWeapon->GetWeaponTag();
	}
	else
	{
		EquippedWeaponTag = FGameplayTag();
	}
}

void UTimeThiefFirstPersonAnimInstance::UpdateSway(float DeltaSeconds)
{
	if (!PlayerCharacter) return;

	FRotator CurrentRotation = PlayerCharacter->GetControlRotation();
	FRotator DeltaRot = CurrentRotation - LastRotation;
	DeltaRot.Normalize();

	float TargetPitch = FMath::Clamp(DeltaRot.Pitch * -1.0f, -MaxSwayDegree, MaxSwayDegree);
	float TargetYaw = FMath::Clamp(DeltaRot.Yaw * 1.0f, -MaxSwayDegree, MaxSwayDegree);
	
	FRotator TargetSwayRot(TargetPitch, TargetYaw, TargetYaw * 0.5f);

	SwayRotation = FMath::RInterpTo(SwayRotation, TargetSwayRot, DeltaSeconds, SwaySpeed);

	LastRotation = CurrentRotation;
}
