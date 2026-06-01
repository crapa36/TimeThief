#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Smoke/TimeThiefSmokeTypes.h"
#include "TimeThiefSmokeRendererTypes.h"
#include "TimeThiefSmokeVolume.generated.h"

class UBoxComponent;
class UPrimitiveComponent;

UCLASS()
class TIMETHIEF_API ATimeThiefSmokeVolume : public AActor
{
	GENERATED_BODY()

public:
	ATimeThiefSmokeVolume();

	void InitializeSmokeVolume(AActor* InOwnerActor, APawn* InInstigatorPawn);

	int32 GetSmokeId() const { return SmokeId; }
	float GetSmokeAgeSeconds() const { return SmokeAgeSeconds; }
	FVector GetCurrentSmokeBoundsExtent() const;
	FVector GetCurrentSmokeRenderBoundsExtent() const;
	FBox GetCurrentSmokeWorldBounds() const;
	int32 GetObstacleFieldResolution() const { return ObstacleFieldResolution; }
	uint32 GetObstacleFieldRevision() const { return ObstacleFieldRevision; }
	const TArray<FTimeThiefSmokeObstaclePrimitive>& GetObstaclePrimitives() const { return ObstaclePrimitives; }
	bool HasSolidObstacleField() const { return bHasSolidObstacleField; }
	void FlushPendingObstacleFieldRebuild(float DeltaTime);
	void GatherActorPushEventsFromSamples(const TArray<FTimeThiefSmokeActorPushSample>& CandidateSamples);

	bool IntersectTraceSegment(const FVector& SegmentStart, const FVector& SegmentEnd, FVector& OutEntryPoint, FVector& OutExitPoint) const;
	bool IntersectsExplosion(const FVector& Center, float Radius) const;

	void HandleBulletTrace(const FVector& EntryPoint, const FVector& ExitPoint, float Strength, int32 Seed);
	void HandleExplosionShock(const FVector& Center, float Radius, float Strength, int32 Seed);
	void ApplyInteractionEvent(const FTimeThiefSmokeInteractionEvent& Event);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TimeThief|Smoke")
	TObjectPtr<UBoxComponent> SmokeBoundsComponent;

private:
	void MarkObstacleFieldDirty();
	bool HasTrackedDynamicObstacleChanged() const;
	void RebuildStaticObstacleField(float DeltaTime = 0.0f);
	void UpdateSmokeBounds();
	void NotifySpatialBoundsIfChanged();

	UPROPERTY(VisibleInstanceOnly, Category = "TimeThief|Smoke|Runtime")
	int32 SmokeId = INDEX_NONE;

	TMap<TWeakObjectPtr<UPrimitiveComponent>, FTransform> PreviousObstacleComponentTransforms;
	float SmokeAgeSeconds = 0.0f;
	TArray<FTimeThiefSmokeObstaclePrimitive> ObstaclePrimitives;
	uint64 ObstacleFieldSignature = 0;
	int32 ObstacleFieldResolution = 0;
	uint32 ObstacleFieldRevision = 0;
	float ObstacleDynamicRefreshAccumulator = 0.0f;
	bool bHasBuiltObstacleField = false;
	bool bHasSolidObstacleField = false;
	bool bObstacleFieldRebuildPending = false;
	FBox LastSmokeSpatialBounds = FBox(EForceInit::ForceInit);
};
