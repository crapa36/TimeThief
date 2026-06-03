#include "Animation/Player/TimeThiefFirstPersonAnimInstance.h"
#include "Character/TimeThiefPlayerCharacter.h"
#include "Components/Combat/TimeThiefPawnCombatComponent.h"
#include "Weapon/Components/TimeThiefWeaponComponentBase.h"
#include "GameFramework/Controller.h"

UTimeThiefFirstPersonAnimInstance::UTimeThiefFirstPersonAnimInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bHasWeapon = false;
	SwayRotation = FRotator::ZeroRotator;
	ProceduralSpeed = 0.0f;
	ProceduralVelocity = FVector::ZeroVector;
	DeltaRotation = FRotator::ZeroRotator;
	AccumulatedTime = 0.0f;
	CurrentBobAmplitude = 0.0f;
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
	// Super::NativeUpdateAnimation(DeltaSeconds);

	// if (!PlayerCharacter)
	// {
	// 	return;
	// }

	// UpdateWeaponData();
	// UpdateSway(DeltaSeconds);
	// UpdateProceduralData(DeltaSeconds);
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

	UTimeThiefPawnCombatComponent* CombatComp = PlayerCharacter->GetCombatComponent();
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

	float TargetPitch = FMath::Clamp(-DeltaRot.Pitch, -MaxSwayDegree, MaxSwayDegree);
	float TargetYaw = FMath::Clamp(DeltaRot.Yaw, -MaxSwayDegree, MaxSwayDegree);
	
	FRotator TargetSwayRot(TargetPitch, TargetYaw, TargetYaw * 0.5f);

	SwayRotation = FMath::RInterpTo(SwayRotation, TargetSwayRot, DeltaSeconds, SwaySpeed);

	DeltaRotation = DeltaRot;

	LastRotation = CurrentRotation;
}

void UTimeThiefFirstPersonAnimInstance::UpdateProceduralData(float DeltaSeconds)
{
	if (!PlayerCharacter) return;

	AccumulatedTime += DeltaSeconds;

	ProceduralVelocity = Velocity;
	ProceduralSpeed = ProceduralVelocity.Size2D();

	float TargetBobAmplitude = IdleBobAmplitude;
	if (ProceduralSpeed > RunSpeedThreshold)
	{
		TargetBobAmplitude = RunBobAmplitude;
	}
	else if (ProceduralSpeed > WalkSpeedThreshold)
	{
		float Alpha = (ProceduralSpeed - WalkSpeedThreshold) / (RunSpeedThreshold - WalkSpeedThreshold);
		TargetBobAmplitude = FMath::Lerp(WalkBobAmplitude, RunBobAmplitude, Alpha);
	}
	else if (ProceduralSpeed > 10.0f)
	{
		float Alpha = ProceduralSpeed / WalkSpeedThreshold;
		TargetBobAmplitude = FMath::Lerp(IdleBobAmplitude, WalkBobAmplitude, Alpha);
	}

	CurrentBobAmplitude = FMath::FInterpTo(CurrentBobAmplitude, TargetBobAmplitude, DeltaSeconds, BobAmplitudeInterpSpeed);
}
