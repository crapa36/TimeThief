#pragma once

#include "CoreMinimal.h"

struct TIMETHIEFSMOKERENDERER_API FTimeThiefSmokeTestEvent
{
	FString Type;
	FString ActionId;
	FString EntityId;
	FString Label;
	FString Shape;
	FString ActorName;
	FString ComponentName;
	uint64 FrameId = 0;
	int32 SmokeId = INDEX_NONE;
	int32 ItemIndex = INDEX_NONE;
	int32 Count = 0;
	int32 Seed = 0;
	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
	FVector Entry = FVector::ZeroVector;
	FVector Exit = FVector::ZeroVector;
	FVector Position = FVector::ZeroVector;
	FVector PreviousPosition = FVector::ZeroVector;
	FVector Direction = FVector::ZeroVector;
	FVector Extents = FVector::ZeroVector;
	double Radius = 0.0;
	double Length = 0.0;
	double Strength = 0.0;
	double Speed = 0.0;
	TArray<int32> SmokeIds;
};

struct TIMETHIEFSMOKERENDERER_API FTimeThiefSmokeTestGpuPassResult
{
	uint64 FrameId = 0;
	uint32 SimulationStep = 0;
	FString Phase;
	FName PassName;
	int32 SmokeId = INDEX_NONE;
	int32 BatchIndex = INDEX_NONE;
	int32 BatchCount = 0;
	int32 IterationIndex = INDEX_NONE;
	int32 EventCount = 0;
	int32 SmokeCount = 0;
	int32 BulletEventCount = 0;
	int32 ExplosionEventCount = 0;
	int32 ActorEventCount = 0;
	int32 DrawPixelCount = 0;
	int32 ViewportPixelCount = 0;
	FIntRect DrawRect;
	int32 TileCount = 0;
	int32 EmptyTileCount = 0;
	int32 MaxSmokesPerTile = 0;
	float AverageSmokesPerNonEmptyTile = 0.0f;
	TArray<int32> TileSmokeCountHistogram;
	int32 RenderMinSteps = 0;
	int32 RenderMaxSteps = 0;
	int32 EstimatedFullRaySteps = 0;
	float TargetStepLength = 0.0f;
	int32 ActualResolvedStepMin = 0;
	int32 ActualResolvedStepMax = 0;
	float ActualResolvedStepAverage = 0.0f;
	int32 ActualExecutedStepMin = 0;
	int32 ActualExecutedStepMax = 0;
	float ActualExecutedStepAverage = 0.0f;
	FString SampleGridMode;
	float WorldStepLength = 0.0f;
	uint32 SamplePhaseHash = 0;
	uint32 SegmentCount = 0;
	uint32 StableSampleCount = 0;
	uint32 SparseSkipStepCount = 0;
	uint32 CombinedMediumSampleCount = 0;
	uint32 CombinedShadowEvaluationCount = 0;
	int32 CombinedShadowStepCount = 0;
	float CombinedShadowStepLength = 0.0f;
	bool bOrderIndependentIntegrator = false;
	int32 SparseSmokeCount = 0;
	int32 PackedDenseSmokeCount = 0;
	int32 BulletFieldActiveSmokeCount = 0;
	bool bHalfResolution = false;
	bool bFastFilament = false;
	bool bBoundaryShellGate = false;
	bool bSingleSmokeShader = false;
	int32 CameraInsideSmokeCount = INDEX_NONE;
	float NearestSmokeSurfaceDistance = -1.0f;
	TArray<int32> SmokeIds;
	double DurationMilliseconds = 0.0;
};

struct TIMETHIEFSMOKERENDERER_API FTimeThiefSmokeTestProbeRequest
{
	uint64 RequestId = 0;
	FString Label;
	TArray<int32> SmokeIds;
};

struct TIMETHIEFSMOKERENDERER_API FTimeThiefSmokeTestProbeResult
{
	uint64 RequestId = 0;
	FString Label;
	int32 SmokeId = INDEX_NONE;
	float NaturalDensitySum = 0.0f;
	float DisplacedDensitySum = 0.0f;
	FVector DensityCentroid = FVector::ZeroVector;
	float MaxVelocity = 0.0f;
	float BulletCutoutMax = 0.0f;
	float BulletSinkMax = 0.0f;
	uint32 ActiveDensityVoxels = 0;
	uint32 ActiveBulletVoxels = 0;
	float DensityInsideObstacle = 0.0f;
	float MaxNaturalDensity = 0.0f;
	float MaxDisplacedDensity = 0.0f;
	float MaxCombinedDensity = 0.0f;
	uint32 DensityClampViolationVoxels = 0;
	uint32 SolidObstacleVoxels = 0;
};

class TIMETHIEFSMOKERENDERER_API ITimeThiefSmokeTestSink
{
public:
	virtual ~ITimeThiefSmokeTestSink() = default;
	virtual void EnqueueEvent(const FTimeThiefSmokeTestEvent& Event) = 0;
	virtual void EnqueueGpuPass(const FTimeThiefSmokeTestGpuPassResult& Result) = 0;
	virtual void EnqueueProbe(const FTimeThiefSmokeTestProbeResult& Result) = 0;
};

class TIMETHIEFSMOKERENDERER_API FTimeThiefSmokeTestBridge
{
public:
	static bool IsActive();
	static void SetSink(TSharedPtr<ITimeThiefSmokeTestSink, ESPMode::ThreadSafe> Sink);
	static void ClearSink();
	static void Emit(const FTimeThiefSmokeTestEvent& Event);
	static void EmitGpuPass(const FTimeThiefSmokeTestGpuPassResult& Result);
	static void EmitProbe(const FTimeThiefSmokeTestProbeResult& Result);

	static void SetPhase(const FString& Phase);
	static FString GetPhase();
	static void SetMeasurementActive(bool bActive);
	static bool IsMeasurementActive();
	static uint64 RequestProbe(const FString& Label, const TArray<int32>& SmokeIds);
	static bool DequeueProbeRequest(FTimeThiefSmokeTestProbeRequest& OutRequest);
	static void NotifyGpuQueryQueued();
	static void NotifyGpuQueryFinished();
	static int32 GetPendingGpuQueryCount();
	static void NotifyProbeFinished();
	static int32 GetPendingProbeCount();
};
