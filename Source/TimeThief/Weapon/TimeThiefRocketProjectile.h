#pragma once

#include "CoreMinimal.h"
#include "Actors/NetworkActor.h"
#include "TimeThiefRocketProjectile.generated.h"

class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;
class UParticleSystemComponent;
class UAudioComponent;
class UParticleSystem;
class USoundBase;
class APawn;

UCLASS()
class TIMETHIEF_API ATimeThiefRocketProjectile : public ANetworkActor
{
	GENERATED_BODY()

public:
	ATimeThiefRocketProjectile();

	void InitializeProjectile(AActor* InOwnerActor, APawn* InInstigatorPawn);
	void ActivateProjectile(const FTransform& SpawnTransform);
	void DeactivateProjectile();
	bool IsActive() const { return bIsActivated && !bExploded; }
	void SetDamageBonus(float InDamageBonus) { DamageBonus = FMath::Max(0.0f, InDamageBonus); }

protected:
	virtual void BeginPlay() override;
	
	virtual void Tick(float DeltaTime) override;

	UFUNCTION()
	void OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	void HandleLifeTimeExpired();
	UFUNCTION()
	void ExplodeOnce(const FHitResult& Hit);
	void ApplyExplosionDamage(const FVector& ExplosionLocation);
	void PlayExplosionEffects(const FVector& ExplosionLocation, const FVector& ExplosionNormal);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TimeThief|Weapon|Rocket")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TimeThief|Weapon|Rocket")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovementComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TimeThief|Weapon|Rocket")
	TObjectPtr<UStaticMeshComponent> ProjectileMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TimeThief|Weapon|Rocket")
	TObjectPtr<UParticleSystemComponent> TrailEffectComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TimeThief|Weapon|Rocket")
	TObjectPtr<UAudioComponent> FlightLoopAudioComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Rocket|Flight", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float CollisionRadius = 12.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Rocket|Flight", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float InitialSpeed = 2000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Rocket|Flight", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MaxSpeed = 3000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Rocket|Flight")
	float GravityScale = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Rocket|Damage")
	float MaxDamage = 120.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Rocket|Damage")
	float MinDamage = 30.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Rocket|Damage")
	float DamageInnerRadius = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Rocket|Damage")
	float ExplosionRadius = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Rocket|Damage")
	float SelfDamageScale = 1.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Rocket|Runtime")
	float DamageBonus = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Rocket|Life", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float MaxLifeTime = 4.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Rocket|Effects")
	TObjectPtr<UParticleSystem> ExplosionEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Rocket|Effects")
	TObjectPtr<USoundBase> ExplosionSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Rocket|Effects")
	TObjectPtr<USoundBase> FlightLoopSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Rocket|Debug", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ExplosionDebugDuration = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Weapon|Rocket|Debug", meta = (ClampMin = "4", UIMin = "4"))
	int32 ExplosionDebugSegments = 24;
	
// Network 관련 API
public:
	void ActivateProjectileFromNetwork(const FVector& SpawnLocation, const FVector& InitialVelocity);
	void ApplyNetworkMovementState(const FNetworkEntityState& EntityState);
	
private:
	FTimerHandle LifeTimeTimerHandle;
	bool bIsActivated = false;
	bool bExploded = false;
	TWeakObjectPtr<AActor> CachedOwnerActor;
	TWeakObjectPtr<APawn> CachedInstigatorPawn;
	
// Network 보간용 변수
	FVector NetworkTargetLocation = FVector::ZeroVector;
	bool bHasNetworkTargetLocation = false;
};