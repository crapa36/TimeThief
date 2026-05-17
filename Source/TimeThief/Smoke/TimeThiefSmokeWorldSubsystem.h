#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Smoke/TimeThiefSmokeTypes.h"
#include "TimeThiefSmokeWorldSubsystem.generated.h"

class ATimeThiefSmokeVolume;

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
	void PublishRendererFrame(float DeltaTime);

	UPROPERTY()
	TArray<TWeakObjectPtr<ATimeThiefSmokeVolume>> ActiveSmokeVolumes;

	UPROPERTY()
	TArray<FTimeThiefActiveSmokeImpulse> ActiveImpulses;

	UPROPERTY()
	TArray<FTimeThiefSmokeInteractionEvent> PendingRendererEvents;

	TSet<uint64> PersistentClusterLinks;

	TMap<ATimeThiefSmokeVolume*, int32> BulletTraceCountsThisTick;
};
