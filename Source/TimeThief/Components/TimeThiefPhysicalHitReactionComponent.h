#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimeThiefPhysicalHitReactionComponent.generated.h"

class ATimeThiefCharacterBase;
class UMorphingMeshComponent;
class UPhysicalAnimationComponent;
class USkeletalMeshComponent;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class TIMETHIEF_API UTimeThiefPhysicalHitReactionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTimeThiefPhysicalHitReactionComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

private:
	struct FActiveReactionBody
	{
		FName BoneName = NAME_None;
		float BlendWeight = 0.0f;
		float StrengthScale = 0.0f;
	};

	UFUNCTION()
	void OnTakePointDamageCallback(AActor* DamagedActor, float Damage, AController* InstigatedBy,
	                               FVector HitLocation, UPrimitiveComponent* HitComponent, FName BoneName,
	                               FVector ShotFromDirection, const UDamageType* DamageType, AActor* DamageCauser);

	UFUNCTION()
	void OnTakeRadialDamageCallback(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
	                                FVector Origin, const FHitResult& HitInfo, AController* InstigatedBy,
	                                AActor* DamageCauser);

	void PlayHitReaction(float Damage, const FVector& HitLocation, const FVector& IncomingDirection,
	                     FName HitBoneName, bool bRadialDamage);
	void StopReaction();

	bool EnsureSimulationReady(USkeletalMeshComponent*& OutSourceMesh, UMorphingMeshComponent*& OutMorphingComponent);
	void ConfigureSimulationMesh(USkeletalMeshComponent* SourceMesh);
	void SyncSimulationMeshTransform(USkeletalMeshComponent* SourceMesh);
	bool EnsureComponentSpaceTransforms(USkeletalMeshComponent* Mesh) const;
	bool IsMorphingInProgress(const UMorphingMeshComponent* MorphingComponent) const;
	void BeginSimulationMeshPresentation(USkeletalMeshComponent* SourceMesh);
	void EndSimulationMeshPresentation();

	FName ResolveHitBodyBone(USkeletalMeshComponent* SourceMesh, FName HitBoneName, const FVector& HitLocation) const;
	FName ResolveClosestReactionBodyBone(USkeletalMeshComponent* SourceMesh, const FVector& HitLocation) const;
	void GatherActiveReactionBodies(USkeletalMeshComponent* SourceMesh, FName HitBodyBone);
	void ApplyActiveBodyBlend(float BlendAlpha);
	float ResolveBodyFalloffWeight(const USkeletalMeshComponent* SourceMesh, FName BodyBoneName, FName HitBodyBone) const;
	int32 ResolveLocalBoneDistance(const USkeletalMeshComponent* SourceMesh, FName FirstBoneName, FName SecondBoneName) const;
	FName GetSkeletonRootBoneName(const USkeletalMeshComponent* Mesh) const;
	bool IsCenterLockedBone(const USkeletalMeshComponent* Mesh, FName BoneName) const;
	void DisableCenterBodyPhysics();
	bool HasPhysicsBody(const USkeletalMeshComponent* Mesh, FName BoneName) const;
	float ResolveImpulseMagnitude(float Damage, bool bRadialDamage) const;

	UPROPERTY(Transient)
	TObjectPtr<UPhysicalAnimationComponent> PhysicalAnimationComponent;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> SimulationMesh;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> CachedSourceMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMorphingMeshComponent> CachedMorphingComponent;

	FTimerHandle StopReactionTimerHandle;
	TArray<FActiveReactionBody> ActiveReactionBodies;
	bool bSourceMeshVisibilityCached = false;
	bool bSourceMeshWasVisible = false;
	bool bSourceMeshWasHiddenInGame = false;
	bool bReactionActive = false;
	float LastReactionTime = -1000.0f;
};
