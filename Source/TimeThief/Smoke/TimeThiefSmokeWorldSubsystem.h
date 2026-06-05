#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Smoke/TimeThiefSmokeTypes.h"
#include "TimeThiefSmokeWorldSubsystem.generated.h"

class ATimeThiefSmokeVolume;
class UPrimitiveComponent;

struct FTimeThiefSmokeSpatialEntry
{
	TWeakObjectPtr<ATimeThiefSmokeVolume> SmokeVolume;
	FBox Bounds = FBox(EForceInit::ForceInit);
};

USTRUCT()
struct FTimeThiefActiveSmokeImpulse
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<ATimeThiefSmokeVolume> SmokeVolume;

	UPROPERTY()
	FTimeThiefSmokeInteractionEvent Event;

	UPROPERTY()
	float Age = 0.0f;

	UPROPERTY()
	float Duration = 0.0f;
};

UCLASS()
class TIMETHIEF_API UTimeThiefSmokeWorldSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

	int32 AllocateSmokeId();
	void RegisterSmokeVolume(ATimeThiefSmokeVolume* SmokeVolume);
	void UnregisterSmokeVolume(ATimeThiefSmokeVolume* SmokeVolume);

	void SubmitBulletTrace(const FVector& TraceStart, const FVector& TraceEnd, float Strength, int32 Seed);
	void SubmitExplosion(const FVector& Center, float Radius, float Strength, int32 Seed);
	void AddTimedInteractionEvent(ATimeThiefSmokeVolume* SmokeVolume, const FTimeThiefSmokeInteractionEvent& Event, float Duration);
	void RecordRendererEvent(const FTimeThiefSmokeInteractionEvent& Event);
	void NotifySmokeVolumeBoundsChanged(ATimeThiefSmokeVolume* SmokeVolume);

private:
	void CompactSmokeVolumes();
	void MarkSmokeSpatialIndexDirty();
	void ValidateSmokeSpatialIndexBounds();
	void RebuildSmokeSpatialIndex();
	void QuerySmokeSpatialIndex(const FBox& QueryBounds, TArray<ATimeThiefSmokeVolume*>& OutSmokeVolumes);
	void GatherActorPushEvents(float DeltaTime);
	uint64 GetRendererSceneKey() const;
	void PublishRendererFrame(float DeltaTime);

	UPROPERTY()
	TArray<TWeakObjectPtr<ATimeThiefSmokeVolume>> ActiveSmokeVolumes;

	UPROPERTY()
	TArray<FTimeThiefActiveSmokeImpulse> ActiveImpulses;

	UPROPERTY()
	TArray<FTimeThiefSmokeInteractionEvent> PendingRendererEvents;

	TMap<ATimeThiefSmokeVolume*, int32> BulletTraceCountsThisTick;
	TMap<TWeakObjectPtr<UPrimitiveComponent>, FVector> PreviousActorPushComponentLocations;

	TArray<FTimeThiefSmokeSpatialEntry> SmokeSpatialEntries;
	TMap<FIntVector, TArray<int32>> SmokeSpatialCells;
	TArray<uint32> SmokeSpatialQueryEntryStamps;
	TArray<int32> SmokeSpatialQueryEntryIndices;
	TArray<ATimeThiefSmokeVolume*> SmokeSpatialQueryResults;
	TMap<int32, uint32> LastPublishedObstacleFieldRevisions;
	uint64 RendererSceneKey = 0;
	uint64 SmokeSpatialIndexValidationFrame = MAX_uint64;
	uint32 SmokeSpatialQueryStamp = 0;
	int32 NextSmokeId = 1;
	float ActorInteractionAccumulator = 0.0f;
	bool bSmokeSpatialIndexDirty = true;
};
