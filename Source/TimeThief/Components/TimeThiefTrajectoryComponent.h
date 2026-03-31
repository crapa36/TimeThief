#pragma once

#include "CoreMinimal.h"
#include "CharacterTrajectoryComponent.h"
#include "TimeThiefTrajectoryComponent.generated.h"

struct FRemoteNetSample
{
	double TimeSeconds = 0.0;
	FVector Position = FVector::ZeroVector;
	float YawDeg = 0.f;
};

class FRemoteTrajectoryHistory
{
public:
	explicit FRemoteTrajectoryHistory(double InMaxHistorySeconds = 3.0);

	void SetMaxHistorySeconds(double InMaxHistorySeconds);
	void AddSample(double TimeSeconds, const FVector& Position, float YawDeg);
	bool SampleAt(double QueryTime, FVector& OutPos, float& OutYawDeg) const;
	bool GetLastTwo(FRemoteNetSample& OutPrev, FRemoteNetSample& OutCurr) const;
	bool GetLastThree(FRemoteNetSample& OutPrev2, FRemoteNetSample& OutPrev, FRemoteNetSample& OutCurr) const;

private:
	TArray<FRemoteNetSample> Samples;
	double MaxHistorySeconds;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class TIMETHIEF_API UTimeThiefTrajectoryComponent : public UCharacterTrajectoryComponent
{
	GENERATED_BODY()

public:
	UTimeThiefTrajectoryComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	void UpdateRemoteTrajectory(float DeltaTime);
	FVector EstimatePlanarVelocityFromHistory(const FRemoteTrajectoryHistory& History) const;

	UPROPERTY(EditAnywhere, Category = "Remote Trajectory")
	float HistoryLengthSeconds = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Remote Trajectory")
	int32 HistorySamplesPerSecond = 10;

	UPROPERTY(EditAnywhere, Category = "Remote Trajectory")
	float PredictionLengthSeconds = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Remote Trajectory")
	int32 PredictionSamplesPerSecond = 10;

private:
	FRemoteTrajectoryHistory RemoteHistory;
	double SimulatedTime = 0.0;
};