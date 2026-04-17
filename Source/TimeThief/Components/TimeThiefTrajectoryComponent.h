#pragma once

#include "CoreMinimal.h"
#include "CharacterTrajectoryComponent.h"
#include "TimeThiefTrajectoryComponent.generated.h"

struct FRemoteNetSample
{
	double TimeSeconds = 0.0;
	FVector Position = FVector::ZeroVector;
	float YawDeg = 0.f;
	FVector Velocity2D = FVector::ZeroVector;
};

class FRemoteTrajectoryHistory
{
public:
	explicit FRemoteTrajectoryHistory(double InMaxHistorySeconds = 3.0);

	void SetMaxHistorySeconds(double InMaxHistorySeconds);
	void AddSample(double TimeSeconds, const FVector& Position, float YawDeg, const FVector& Velocity2D);
	void ResetToSample(double TimeSeconds, const FVector& Position, float YawDeg, const FVector& Velocity2D);
	bool SampleAt(double QueryTime, FVector& OutPos, float& OutYawDeg, FVector& OutVelocity2D) const;
	bool GetLast(FRemoteNetSample& OutLast) const;
	bool GetLastTwo(FRemoteNetSample& OutPrev, FRemoteNetSample& OutCurr) const;

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

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	void UpdateRemoteTrajectory(float DeltaTime);

	UPROPERTY(EditAnywhere, Category = "Remote Trajectory")
	float HistoryLengthSeconds = 0.7f;

	UPROPERTY(EditAnywhere, Category = "Remote Trajectory")
	int32 HistorySamplesPerSecond = 20;

	UPROPERTY(EditAnywhere, Category = "Remote Trajectory")
	float PredictionLengthSeconds = 0.22f;

	UPROPERTY(EditAnywhere, Category = "Remote Trajectory")
	int32 PredictionSamplesPerSecond = 20;

	UPROPERTY(EditAnywhere, Category = "Remote Trajectory|Snapping")
	float HardSnapDistanceCm = 120.0f;

	UPROPERTY(EditAnywhere, Category = "Remote Trajectory|Snapping")
	float SoftSnapDistanceCm = 35.0f;

	UPROPERTY(EditAnywhere, Category = "Remote Trajectory|Snapping")
	float HardSnapYawDeg = 25.0f;

	UPROPERTY(EditAnywhere, Category = "Remote Trajectory|Snapping")
	float SnapCooldownSeconds = 0.05f;

	UPROPERTY(EditAnywhere, Category = "Remote Trajectory|Snapping")
	float ForceHardSnapDistanceCm = 350.0f;

	UPROPERTY(EditAnywhere, Category = "Remote Trajectory|Snapping", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HardSnapBlendAlpha = 0.75f;

	UPROPERTY(EditAnywhere, Category = "Remote Trajectory|Prediction")
	float PredictionYawRateClampDegPerSec = 540.0f;

	UPROPERTY(EditAnywhere, Category = "Remote Trajectory|Prediction")
	float HighTurnYawRateThresholdDegPerSec = 180.0f;

	UPROPERTY(EditAnywhere, Category = "Remote Trajectory|Prediction")
	float HighTurnYawRateClampDegPerSec = 140.0f;

	UPROPERTY(EditAnywhere, Category = "Remote Trajectory|Prediction")
	float LowSpeedPredictionThresholdCmPerSec = 220.0f;

	UPROPERTY(EditAnywhere, Category = "Remote Trajectory|Prediction", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LowSpeedPredictionVelocityScale = 0.05f;

	UPROPERTY(EditAnywhere, Category = "Remote Trajectory|Prediction", meta = (ClampMin = "0.0"))
	float PredictionVelocityDamping = 4.0f;

	UPROPERTY(EditAnywhere, Category = "Remote Trajectory|Velocity", meta = (ClampMin = "0.0"))
	float HistoryVelocityZeroThresholdCmPerSec = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Remote Trajectory|Velocity", meta = (ClampMin = "0.0"))
	float PredictionVelocityZeroThresholdCmPerSec = 4.0f;

	UPROPERTY(EditAnywhere, Category = "Remote Trajectory|Velocity", meta = (ClampMin = "0.0"))
	float MinObservedDisplacementCm = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Remote Trajectory|Velocity", meta = (ClampMin = "0.0"))
	float VelocitySmoothingResponse = 15.0f;

	UPROPERTY(EditAnywhere, Category = "Remote Trajectory|Velocity", meta = (ClampMin = "0.0"))
	float SmoothedVelocityZeroThresholdCmPerSec = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Remote Trajectory|Prediction", meta = (ClampMin = "0.0"))
	float YawRateDeadZoneDegPerSec = 0.1f;

private:
	FRemoteTrajectoryHistory RemoteHistory;
	double SimulatedTime = 0.0;
	FVector LastSmoothedVelocity = FVector::ZeroVector;
	double LastSnapTimeSeconds = -1000000.0;
	int32 HardSnapCount = 0;
	int32 SoftSnapCount = 0;
};