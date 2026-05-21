#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Smoke/TimeThiefSmokeTypes.h"
#include "TimeThiefSmokeWorldSubsystem.generated.h"

class ATimeThiefSmokeVolume;

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
	virtual TStatId GetStatId() const override;

	void RegisterSmokeVolume(ATimeThiefSmokeVolume* SmokeVolume);
	void UnregisterSmokeVolume(ATimeThiefSmokeVolume* SmokeVolume);

	void SubmitBulletTrace(const FVector& TraceStart, const FVector& TraceEnd, float Strength, int32 Seed);
	void SubmitExplosion(const FVector& Center, float Radius, float Strength, int32 Seed);
	void AddTimedInteractionEvent(ATimeThiefSmokeVolume* SmokeVolume, const FTimeThiefSmokeInteractionEvent& Event, float Duration);
	void RecordRendererEvent(const FTimeThiefSmokeInteractionEvent& Event);

private:
	void CompactSmokeVolumes();
	void MarkSmokeSpatialIndexDirty();
	void RebuildSmokeSpatialIndex();
	void QuerySmokeSpatialIndex(const FBox& QueryBounds, TArray<ATimeThiefSmokeVolume*>& OutSmokeVolumes);
	void PublishRendererFrame(float DeltaTime);

	UPROPERTY()
	TArray<TWeakObjectPtr<ATimeThiefSmokeVolume>> ActiveSmokeVolumes;

	UPROPERTY()
	TArray<FTimeThiefActiveSmokeImpulse> ActiveImpulses;

	UPROPERTY()
	TArray<FTimeThiefSmokeInteractionEvent> PendingRendererEvents;

	TSet<uint64> PersistentClusterLinks;

	TMap<ATimeThiefSmokeVolume*, int32> BulletTraceCountsThisTick;

	TArray<FTimeThiefSmokeSpatialEntry> SmokeSpatialEntries;
	TMap<FIntVector, TArray<int32>> SmokeSpatialCells;
	uint64 SmokeSpatialIndexFrame = MAX_uint64;
	bool bSmokeSpatialIndexDirty = true;
};
