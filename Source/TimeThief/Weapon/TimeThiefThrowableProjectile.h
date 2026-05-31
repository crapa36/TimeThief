#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ItemCommons.h"
#include "Actors/NetworkActor.h"
#include "Weapon/TimeThiefThrowableTypes.h"

#include "TimeThiefThrowableProjectile.generated.h"

class APawn;
class ATimeThiefSmokeVolume;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;
class UTimeThiefWeaponTrail;
class UNiagaraComponent;
class UThrowableNetworkSyncComponent;
struct FThrowableMoveSnapshot;

UCLASS()
class TIMETHIEF_API ATimeThiefThrowableProjectile : public ANetworkActor
{
	GENERATED_BODY()

public:
	ATimeThiefThrowableProjectile();

	void InitializeThrowable(EItemID InItemID, AActor* InOwnerActor, APawn* InInstigatorPawn);
	void InitializeThrowable(EItemID InItemID, AActor* InOwnerActor, APawn* InInstigatorPawn, const FTimeThiefThrowableProjectileSettings& InProjectileSettings);
	void LaunchThrowable(const FVector& InitialVelocity, float InFuseTime);
	void SetThrowableMesh();

public:
	void InitializeNetworkSyncAsLocalOwner(uint32 ObjectId);
	void InitializeNetworkSyncAsRemoteProxy(uint32 ObjectId);
	void PushRemoteMoveSnapshot(const FThrowableMoveSnapshot& Snapshot);
	
	void RemoteExplosionEffect(const FVector& ExplosionLocation);
	
protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void HandleFuseExpired();
	void ExplodeOnce();
	void ApplyRadialThrowableDamage(const FVector& ExplosionLocation);
	void PlayDetonationEffects(const FVector& ExplosionLocation);
	void SpawnSmokeVolume(const FVector& SmokeLocation);
	void ApplyProjectileSettings();
	void StartGrenadeTrail();
	void PlayCollisionSound(const FHitResult& ImpactResult, const FVector& ImpactVelocity);

	UFUNCTION()
	void OnProjectileBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TimeThief|Throwable")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TimeThief|Throwable")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TimeThief|Throwable")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovementComponent;
	
	// Network용 Projectile Movement Sync 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="TimeThief|Throwable|Network")
	TObjectPtr<UThrowableNetworkSyncComponent> ThrowableNetworkSyncComponent;

	UPROPERTY(Transient)
	TObjectPtr<UTimeThiefWeaponTrail> WeaponTrail;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> ActiveTrailComponent;

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
