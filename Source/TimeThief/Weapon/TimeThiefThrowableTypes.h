#pragma once

#include "CoreMinimal.h"
#include "ItemCommons.h"
#include "TimeThiefThrowableTypes.generated.h"

class UAnimSequenceBase;
class UNiagaraSystem;
class USoundBase;
class UStaticMesh;

USTRUCT(BlueprintType)
struct FTimeThiefThrowableThrowSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Throwable|Throw", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ThrowSpeed = 1200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Throwable|Throw")
	float ThrowUpwardVelocity = 250.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Throwable|Throw", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float FuseTime = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Throwable|Throw", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ThrowCooldown = 0.75f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Throwable|Animation")
	TObjectPtr<UAnimSequenceBase> ThrowAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Throwable|Animation")
	FName ThrowAnimSlot = FName("DefaultSlot");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Throwable|Sound")
	TObjectPtr<USoundBase> ThrowSound;
};

USTRUCT(BlueprintType)
struct FTimeThiefThrowableProjectileSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Throwable|Visual")
	TObjectPtr<UStaticMesh> MeshOverride;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Throwable|Visual", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float MeshVisualRadius = 12.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Throwable|Flight", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float CollisionRadius = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Throwable|Flight")
	float GravityScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Throwable|Flight")
	bool bShouldBounce = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Throwable|Flight")
	float Bounciness = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Throwable|Flight")
	float Friction = 0.55f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Throwable|Damage")
	bool bApplyRadialDamage = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Throwable|Damage")
	float MaxDamage = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Throwable|Damage")
	float MinDamage = 30.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Throwable|Damage")
	float DamageInnerRadius = 240.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Throwable|Damage")
	float DamageOuterRadius = 480.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Throwable|Effects")
	TObjectPtr<UNiagaraSystem> DetonationNiagaraEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Throwable|Sound")
	TObjectPtr<USoundBase> ExplosionSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Throwable|Sound")
	TObjectPtr<USoundBase> CollisionSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Throwable|Sound", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float CollisionSoundMinSpeed = 150.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Throwable|Sound", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float CollisionSoundCooldown = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Throwable|Debug")
	bool bDrawDamageDebug = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Throwable|Debug", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float DamageDebugDuration = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Throwable|Debug", meta = (ClampMin = "4", UIMin = "4"))
	int32 DebugSegments = 32;
};

USTRUCT(BlueprintType)
struct FTimeThiefThrowableDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Throwable")
	FTimeThiefThrowableThrowSettings ThrowSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Throwable")
	FTimeThiefThrowableProjectileSettings ProjectileSettings;
};
