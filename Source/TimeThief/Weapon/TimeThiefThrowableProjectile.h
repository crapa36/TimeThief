#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ItemCommons.h"
#include "Smoke/TimeThiefSmokeTypes.h"
#include "Weapon/TimeThiefThrowableTypes.h"
#include "TimeThiefThrowableProjectile.generated.h"

class APawn;
class ATimeThiefSmokeVolume;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

UCLASS()
class TIMETHIEF_API ATimeThiefThrowableProjectile : public AActor
{
	GENERATED_BODY()

public:
	ATimeThiefThrowableProjectile();

	void InitializeThrowable(EItemID InItemID, AActor* InOwnerActor, APawn* InInstigatorPawn);
	void InitializeThrowable(EItemID InItemID, AActor* InOwnerActor, APawn* InInstigatorPawn, const FTimeThiefThrowableProjectileSettings& InProjectileSettings);
	void LaunchThrowable(const FVector& InitialVelocity, float InFuseTime);
	void SetThrowableMesh();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void HandleFuseExpired();
	void ExplodeOnce();
	void ApplyRadialThrowableDamage(const FVector& ExplosionLocation);
	void PlayDetonationEffects(const FVector& ExplosionLocation);
	void SpawnSmokeVolume(const FVector& SmokeLocation);
	FTimeThiefSmokeRuntimeSettings BuildSmokeRuntimeSettings() const;
	void ApplyProjectileSettings();
	void PlayCollisionSound(const FHitResult& ImpactResult, const FVector& ImpactVelocity);

	UFUNCTION()
	void OnProjectileBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TimeThief|Throwable")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TimeThief|Throwable")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TimeThief|Throwable")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovementComponent;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "TimeThief|Throwable|Runtime")
	EItemID ThrowableItemID = EItemID::SIZE;

private:
	FTimerHandle FuseTimerHandle;
	bool bExploded = false;
	FTimeThiefThrowableProjectileSettings ActiveSettings;
	float LastCollisionSoundTime = -100000.0f;
	TWeakObjectPtr<AActor> CachedOwnerActor;
	TWeakObjectPtr<APawn> CachedInstigatorPawn;
};
