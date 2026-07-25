#pragma once

#include "CoreMinimal.h"
#include "RHIGPUReadback.h"
#include "SceneViewExtension.h"
#include "TimeThiefSmokeRendererTypes.h"
#include "TimeThiefSmokeTestGpuProfiler.h"

struct FPostProcessMaterialInputs;
struct FScreenPassTexture;
class FRDGBuffer;
class FRDGPooledBuffer;
struct IPooledRenderTarget;

class FTimeThiefSmokeViewExtension : public FSceneViewExtensionBase
{
public:
	explicit FTimeThiefSmokeViewExtension(const FAutoRegister& AutoRegister);

	void SubmitFrame_RenderThread(FTimeThiefSmokeRendererFrame&& Frame);
	void Clear_RenderThread();
	void PreAllocateWarmupTextures_RenderThread(FRHICommandListImmediate& RHICmdList);

	virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override;
	virtual void SubscribeToPostProcessingPass(EPostProcessingPass Pass, const FSceneView& InView, FPostProcessingPassDelegateArray& InOutPassCallbacks, bool bIsPassEnabled) override;

	FScreenPassTexture CompositeSmoke_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs);

private:
	struct FRenderSmokeStateKey
	{
		uint64 SceneKey = 0;
		int32 SmokeId = INDEX_NONE;

		friend bool operator==(const FRenderSmokeStateKey& Left, const FRenderSmokeStateKey& Right)
		{
			return Left.SceneKey == Right.SceneKey && Left.SmokeId == Right.SmokeId;
		}

		friend uint32 GetTypeHash(const FRenderSmokeStateKey& Key)
		{
			return HashCombineFast(::GetTypeHash(Key.SceneKey), ::GetTypeHash(Key.SmokeId));
		}
	};

	struct FRenderSmokeState
	{
		uint64 SceneKey = 0;
		FTimeThiefSmokeRendererVolume Volume;
		TArray<FTimeThiefSmokeRendererEvent> PendingEvents;
		TRefCountPtr<IPooledRenderTarget> DensityTextures[2];
		TRefCountPtr<IPooledRenderTarget> DisplacedDensityTextures[2];
		TRefCountPtr<IPooledRenderTarget> VelocityTextures[2];
		TRefCountPtr<IPooledRenderTarget> BulletCutoutTextures[2];
		TRefCountPtr<IPooledRenderTarget> BulletSinkTextures[2];
		TRefCountPtr<IPooledRenderTarget> ObstacleSdfTexture;
		TRefCountPtr<IPooledRenderTarget> ObstacleVelocityTexture;
		TRefCountPtr<IPooledRenderTarget> ObstacleFaceOpenTexture;
		TRefCountPtr<IPooledRenderTarget> BrickOccupancyTexture;
		TRefCountPtr<IPooledRenderTarget> PackedDenseFieldTextures[2];
		TRefCountPtr<IPooledRenderTarget> RenderOccupancyTexture;
		TRefCountPtr<IPooledRenderTarget> ExtinctionTextures[2];
		TRefCountPtr<IPooledRenderTarget> LightOpticalDepthTextures[2];
		TRefCountPtr<FRDGPooledBuffer> VortexParticleBuffers[2];
		TRefCountPtr<FRDGPooledBuffer> ProjectionDiagnosticsBuffer;
		int32 CurrentDensityIndex = 0;
		int32 CurrentVelocityIndex = 0;
		int32 CurrentBulletFieldIndex = 0;
		int32 CurrentVortexParticleIndex = 0;
		int32 CurrentPackedFieldIndex = 0;
		FIntVector AllocatedGridSize = FIntVector::ZeroValue;
		FIntVector AllocatedBrickGridSize = FIntVector::ZeroValue;
		FIntVector AllocatedObstacleGridSize = FIntVector::ZeroValue;
		int32 AllocatedMaxActiveBrickCount = 0;
		TUniquePtr<FRHIGPUBufferReadback> SparseActiveBrickCountReadback;
		TUniquePtr<FRHIGPUBufferReadback> VelocityMaximumReadback;
		uint32 SparseActiveBrickCount = 0;
		float LastMeasuredMaxVelocity = 0.0f;
		uint32 LastVelocityMeasurementFrame = 0;
		bool bVelocityMaximumReadbackPending = false;
		uint32 UploadedObstacleFieldRevision = MAX_uint32;
		uint32 LastSimulatedFrame = MAX_uint32;
		int32 AllocatedVortexParticleCount = 0;
		float AccumulatedSimulationDeltaSeconds = 0.0f;
		float SimulationTimeSeconds = 0.0f;
		float RenderTimeSeconds = 0.0f;
		float RenderInterpolationAlpha = 1.0f;
		float AccumulatedVortexDeltaSeconds = 0.0f;
		float VortexActivityBudgetSeconds = 0.0f;
		float BulletFieldDecayBudgetSeconds = 0.0f;
		uint64 SimulationStepId = 0;
		uint64 ProjectionDiagnosticsStepId = MAX_uint64;
		uint32 ProjectionDiagnosticsGraphFrame = MAX_uint32;
		FRDGBuffer* ProjectionDiagnosticsGraphBuffer = nullptr;
		float ProjectionDiagnosticsSimulationInterval = 0.0f;
		float ProjectionDiagnosticsMinCellSize = 1.0f;
		bool bBulletFieldsActive = false;
		bool bNeedsInit = true;
		bool bPackedFieldInitialized = false;
		bool bPackedFieldHistoryValid = false;
		bool bLightVolumeValid = false;
		bool bLightVolumeHistoryValid = false;
		bool bVortexParticlesNeedUpload = true;
		bool bSparseOccupancyRefreshPending = false;
		bool bSparseActiveBrickCountReadbackPending = false;
		bool bUseSparseSimulationMaskThisFrame = false;
	};

	struct FTemporalHistory
	{
		TRefCountPtr<IPooledRenderTarget> SmokeTexture;
		TRefCountPtr<IPooledRenderTarget> DepthTexture;
		FIntPoint Extent = FIntPoint::ZeroValue;
		FMatrix44f PreviousViewProjection;
		FVector3f PreviousViewOrigin = FVector3f::ZeroVector;
		uint32 LastFrame = MAX_uint32;
		bool bValid = false;
	};

	struct FActiveBrickDispatchResources
	{
		FRDGBufferRef ActiveBrickCountBuffer = nullptr;
		FRDGBufferRef ActiveBricksBuffer = nullptr;
		FRDGBufferRef DispatchArgsBuffer = nullptr;
	};

	struct FRetiredSparseActiveBrickCountReadback
	{
		TUniquePtr<FRHIGPUBufferReadback> Readback;
		uint32 RetiredFrameNumber = 0;
	};

	struct FPendingSmokeTestProbeReadback
	{
		TUniquePtr<FRHIGPUBufferReadback> Readback;
		TUniquePtr<FRHIGPUBufferReadback> ProjectionDiagnosticsReadback;
		uint64 RequestId = 0;
		FString Label;
		int32 SmokeId = INDEX_NONE;
		FTransform3f LocalToWorld = FTransform3f::Identity;
		uint64 QueuedFrame = 0;
		float ProjectionSimulationInterval = 0.0f;
		float ProjectionMinCellSize = 1.0f;
	};

	FScreenPassTexture CompositeSmokeMulti_RenderThread(
		FRDGBuilder& GraphBuilder,
		const FSceneView& View,
		const FPostProcessMaterialInputs& Inputs,
		const TArray<FRenderSmokeState*>& RenderStates,
		const TArray<FIntRect>& RenderRects,
		FScreenPassTexture CurrentSceneColor,
		const FMatrix44f& InvViewProjection,
		FRDGTextureRef CompositeTarget,
		int32 ResolutionDivisor,
		int32 BatchIndex = INDEX_NONE,
		int32 BatchCount = 0);

	FRDGTextureRef TemporalResolveSmoke_RenderThread(
		FRDGBuilder& GraphBuilder,
		const FSceneView& View,
		const FPostProcessMaterialInputs& Inputs,
		FRDGTextureRef CurrentSmokeTexture,
		FIntPoint CurrentSmokeExtent,
		FIntPoint FullResolutionExtent);

	FScreenPassTexture BilateralUpsampleSmoke_RenderThread(
		FRDGBuilder& GraphBuilder,
		const FSceneView& View,
		const FPostProcessMaterialInputs& Inputs,
		FScreenPassTexture CurrentSceneColor,
		FRDGTextureRef HalfResSmokeTexture,
		FIntPoint HalfResExtent,
		bool bUseBilateralFilter,
		bool bAllowOverrideOutput);

	void EnsureResources(FRDGBuilder& GraphBuilder, FRenderSmokeState& State);
	void EnsureObstacleFieldTextures(FRDGBuilder& GraphBuilder, FRenderSmokeState& State);
	void EnsureVortexParticleBuffers(FRDGBuilder& GraphBuilder, FRenderSmokeState& State);
	void EnsureDetailNoiseTexture(FRDGBuilder& GraphBuilder);
	void ConsumeSparseActiveBrickCountReadback(FRenderSmokeState& State);
	void ConsumeVelocityMaximumReadback(FRenderSmokeState& State);
	void QueueVelocityMaximumReadback(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef VelocityTexture);
	void QueueSparseActiveBrickCountReadback(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGBufferRef ActiveBrickCountBuffer);
	void RetireSparseActiveBrickCountReadback(FRenderSmokeState& State);
	void ReleaseReadyRetiredSparseActiveBrickCountReadbacks();
	void ProcessSmokeTestProbeRequests(FRDGBuilder& GraphBuilder, uint64 SceneKey);
	void ConsumeSmokeTestProbeReadbacks();
	bool HasRenderableSceneState_RenderThread(uint64 SceneKey, const FSceneViewFamily* ViewFamily = nullptr) const;
	void UploadDeadVortexParticles(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGBufferRef VortexBuffer);
	void AddVortexParticleUpdatePass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGBufferRef VortexIn, FRDGBufferRef VortexOut, FRDGTextureRef DensityIn, FRDGTextureRef DisplacedDensityIn, FRDGTextureRef VelocityIn, FRDGTextureRef BulletCutoutTexture, FRDGTextureRef BulletSinkTexture, FRDGBufferRef EventBuffer, int32 EventCount, float DeltaSeconds);
	FRDGBufferRef AddBuildVortexBrickMasksPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGBufferRef VortexBuffer);
	void AddVortexParticleSplatPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGBufferRef VortexBuffer, FRDGBufferRef VortexBrickMasksBuffer, FRDGTextureRef DensityIn, FRDGTextureRef DisplacedDensityIn, FRDGTextureRef VelocityIn, FRDGTextureRef BulletCutoutTexture, FRDGTextureRef BulletSinkTexture, FRDGTextureRef VelocityOut, float DeltaSeconds);
	void AddBuildEventBrickMasksPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGBufferRef AdvectionEventBuffer, int32 AdvectionEventCount, FRDGBufferRef ExplosionEventBuffer, int32 ExplosionEventCount, FRDGBufferRef ActorEventBuffer, int32 ActorEventCount, FRDGBufferRef VortexEventBuffer, int32 VortexEventCount, FRDGBufferRef& EmptyEventBuffer, FRDGBufferRef& EmptyEventBrickMasksBuffer, FRDGBufferRef& OutAdvectionEventBrickMasksBuffer, FRDGBufferRef& OutExplosionEventBrickMasksBuffer, FRDGBufferRef& OutActorEventBrickMasksBuffer, FRDGBufferRef& OutVortexEventBrickMasksBuffer);
	void AddBuildBrickOccupancyPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef DensityTexture, FRDGTextureRef DisplacedDensityTexture, FRDGTextureRef VelocityTexture, FRDGTextureRef BulletCutoutTexture, FRDGTextureRef BulletSinkTexture, FRDGBufferRef EventBuffer, int32 EventCount, FRDGTextureRef BrickActivityTexture, bool bCheckBulletChannels);
	FActiveBrickDispatchResources AddExpandBrickOccupancyPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef BrickActivityTexture, FRDGTextureRef BrickOccupancyTexture, uint32 MaxActiveListBricks, uint32 MaxDispatchBrickCount);
	FActiveBrickDispatchResources AddBuildActiveBrickListPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef BrickOccupancyTexture);
	FRDGBufferRef AddBuildSparseBrickDispatchArgsPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, const FActiveBrickDispatchResources& ActiveBrickResources, uint32 GroupsPerBrick, uint32 MaxDispatchBrickCount);
	void AddPackDenseFieldPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef DensityTexture, FRDGTextureRef DisplacedDensityTexture, FRDGTextureRef BulletCutoutTexture, FRDGTextureRef BulletSinkTexture, bool bPackBulletChannels);
	void AddBuildRenderOccupancyPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State);
	void AddBuildExtinctionVolumePass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State);
	void AddBuildLightVolumePass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State);
	void SimulateSmoke(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, float DeltaSeconds, float EventDeltaSeconds, bool bFinalizeFluidStep, bool bIsFinalSimulationSubstep);
	void AddInitPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef DensityTexture, FRDGTextureRef DisplacedDensityTexture, FRDGTextureRef VelocityTexture);
	void AddApplyEventsPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef DensityIn, FRDGTextureRef DisplacedDensityIn, FRDGTextureRef VelocityIn, FRDGTextureRef DensityOut, FRDGTextureRef DisplacedDensityOut, FRDGTextureRef VelocityOut, FRDGBufferRef EventBuffer, FRDGBufferRef EventBrickMasksBuffer, int32 EventCount, float DeltaSeconds, const FActiveBrickDispatchResources* ActiveBrickResources);
	void AddDynamicObstaclePass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef DensityIn, FRDGTextureRef DisplacedDensityIn, FRDGTextureRef VelocityIn, FRDGTextureRef DensityOut, FRDGTextureRef DisplacedDensityOut, FRDGTextureRef VelocityOut, FRDGBufferRef EventBuffer, FRDGBufferRef EventBrickMasksBuffer, int32 EventCount, float DeltaSeconds, const FActiveBrickDispatchResources* ActiveBrickResources);
	void AddSimulatePass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef DensityIn, FRDGTextureRef DisplacedDensityIn, FRDGTextureRef VelocityIn, FRDGTextureRef BulletCutoutTexture, FRDGTextureRef BulletSinkTexture, FRDGBufferRef EventBuffer, FRDGBufferRef EventBrickMasksBuffer, int32 EventCount, FRDGTextureRef DensityOut, FRDGTextureRef DisplacedDensityOut, FRDGTextureRef VelocityOut, FRDGTextureRef BulletCutoutOut, FRDGTextureRef BulletSinkOut, float DeltaSeconds, const FActiveBrickDispatchResources* ActiveBrickResources);
	void AddBuildCurlPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef VelocityIn, FRDGTextureRef CurlOut);
	void AddVorticityPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef DensityIn, FRDGTextureRef DisplacedDensityIn, FRDGTextureRef VelocityIn, FRDGTextureRef CurlTexture, FRDGTextureRef VelocityOut, FRDGBufferRef EventBuffer, FRDGBufferRef EventBrickMasksBuffer, int32 EventCount, float DeltaSeconds);
	void AddBuildMacDivergencePass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef VelocityIn, FRDGTextureRef FaceVelocityUOut, FRDGTextureRef FaceVelocityVOut, FRDGTextureRef FaceVelocityWOut, FRDGTextureRef DivergenceOut, FRDGTextureRef PressureOut, const FActiveBrickDispatchResources* ActiveBrickResources, bool bProjectionDiagnostic = false);
	FRDGTextureRef AddMGPCGPressureSolvePasses(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef DivergenceIn);
	void AddPressureJacobiPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, const FIntVector& GridSize, const FVector3f& CellSize, FRDGTextureRef PressureIn, FRDGTextureRef DivergenceIn, FRDGTextureRef PressureOut, const FActiveBrickDispatchResources* ActiveBrickResources, int32 IterationIndex);
	void AddProjectMacToCollocatedVelocityPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef FaceVelocityUIn, FRDGTextureRef FaceVelocityVIn, FRDGTextureRef FaceVelocityWIn, FRDGTextureRef PressureIn, FRDGTextureRef VelocityOut, const FActiveBrickDispatchResources* ActiveBrickResources);
	void AddPressureResidualPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef PressureIn, FRDGTextureRef DivergenceIn, FRDGTextureRef ResidualOut);
	void AddProjectionDiagnosticsPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef DivergenceBefore, FRDGTextureRef DivergenceAfter, FRDGTextureRef PressureResidual, FRDGTextureRef ProjectedVelocity, float SimulationInterval);

	TRefCountPtr<IPooledRenderTarget> DetailNoiseTexture;
	bool bDetailNoiseInitialized = false;

	TMap<FRenderSmokeStateKey, FRenderSmokeState> SmokeStates;
	TMap<uint64, float> LastFrameDeltaSecondsByScene;
	TArray<FRetiredSparseActiveBrickCountReadback> RetiredSparseActiveBrickCountReadbacks;
	TArray<FPendingSmokeTestProbeReadback> PendingSmokeTestProbeReadbacks;
	TMap<uint64, FTemporalHistory> TemporalHistories;
	FTimeThiefSmokeTestGpuProfiler SmokeTestGpuProfiler;
};
