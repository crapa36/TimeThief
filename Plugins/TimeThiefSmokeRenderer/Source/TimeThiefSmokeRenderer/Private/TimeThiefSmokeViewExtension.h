#pragma once

#include "CoreMinimal.h"
#include "RHIGPUReadback.h"
#include "SceneViewExtension.h"
#include "TimeThiefSmokeRendererTypes.h"
#include "TimeThiefSmokeTestGpuProfiler.h"

struct FPostProcessMaterialInputs;
struct FScreenPassTexture;
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
		TRefCountPtr<IPooledRenderTarget> ObstacleNeighborMaskTexture;
		TRefCountPtr<IPooledRenderTarget> PrevObstacleSdfTexture;
		TRefCountPtr<IPooledRenderTarget> PrevObstacleVelocityTexture;
		TRefCountPtr<IPooledRenderTarget> PrevObstacleFaceOpenTexture;
		TRefCountPtr<IPooledRenderTarget> BrickOccupancyTexture;
		TRefCountPtr<IPooledRenderTarget> SparseFieldAtlasTexture;
		TRefCountPtr<IPooledRenderTarget> PackedDenseFieldTexture;
		TRefCountPtr<FRDGPooledBuffer> VortexParticleBuffers[2];
		int32 CurrentDensityIndex = 0;
		int32 CurrentVelocityIndex = 0;
		int32 CurrentBulletFieldIndex = 0;
		int32 CurrentVortexParticleIndex = 0;
		FIntVector AllocatedGridSize = FIntVector::ZeroValue;
		FIntVector AllocatedBrickGridSize = FIntVector::ZeroValue;
		FIntVector AllocatedSparseAtlasBrickGridSize = FIntVector::ZeroValue;
		FIntVector AllocatedSparseAtlasGridSize = FIntVector::ZeroValue;
		FIntVector AllocatedObstacleGridSize = FIntVector::ZeroValue;
		int32 AllocatedSparseAtlasBrickCapacity = 0;
		TUniquePtr<FRHIGPUBufferReadback> SparseActiveBrickCountReadback;
		uint32 SparseActiveBrickCount = 0;
		uint32 UploadedObstacleFieldRevision = MAX_uint32;
		uint32 UploadedObstacleNeighborMaskRevision = MAX_uint32;
		uint32 LastSimulatedFrame = MAX_uint32;
		int32 AllocatedVortexParticleCount = 0;
		float AccumulatedSimulationDeltaSeconds = 0.0f;
		float AccumulatedVortexDeltaSeconds = 0.0f;
		float VortexActivityBudgetSeconds = 0.0f;
		float BulletFieldDecayBudgetSeconds = 0.0f;
		bool bBulletFieldsActive = false;
		bool bNeedsInit = true;
		bool bVortexParticlesNeedUpload = true;
		bool bSparseAtlasVisibleThisFrame = true;
		bool bSparseAtlasScatterPending = false;
		bool bSparseOccupancyRefreshPending = false;
		bool bSparseActiveBrickCountReadbackPending = false;
		bool bUseSparseSimulationMaskThisFrame = false;
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
		uint64 RequestId = 0;
		FString Label;
		int32 SmokeId = INDEX_NONE;
		FTransform3f LocalToWorld = FTransform3f::Identity;
		uint64 QueuedFrame = 0;
	};

	struct FPendingVortexMaskValidationReadback
	{
		TUniquePtr<FRHIGPUBufferReadback> LegacyReadback;
		TUniquePtr<FRHIGPUBufferReadback> ReverseReadback;
		int32 SmokeId = INDEX_NONE;
		uint32 BrickCount = 0;
		uint64 QueuedFrame = 0;
	};

	FScreenPassTexture CompositeSmokeMulti_RenderThread(
		FRDGBuilder& GraphBuilder,
		const FSceneView& View,
		const FPostProcessMaterialInputs& Inputs,
		const TArray<FRenderSmokeState*>& RenderStates,
		const TArray<FIntRect>& RenderRects,
		FScreenPassTexture CurrentSceneColor,
		const FMatrix44f& InvViewProjection,
		bool bAllowOverrideOutput,
		FRDGTextureRef HalfResTarget = nullptr,
		int32 BatchIndex = INDEX_NONE,
		int32 BatchCount = 0);

	FScreenPassTexture BilateralUpsampleSmoke_RenderThread(
		FRDGBuilder& GraphBuilder,
		const FSceneView& View,
		const FPostProcessMaterialInputs& Inputs,
		FScreenPassTexture CurrentSceneColor,
		FRDGTextureRef HalfResSmokeTexture,
		FIntPoint HalfResExtent,
		bool bAllowOverrideOutput);

	void EnsureResources(FRDGBuilder& GraphBuilder, FRenderSmokeState& State);
	void EnsureObstacleFieldTextures(FRDGBuilder& GraphBuilder, FRenderSmokeState& State);
	void EnsureVortexParticleBuffers(FRDGBuilder& GraphBuilder, FRenderSmokeState& State);
	void ConsumeSparseActiveBrickCountReadback(FRenderSmokeState& State);
	void QueueSparseActiveBrickCountReadback(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGBufferRef ActiveBrickCountBuffer);
	void RetireSparseActiveBrickCountReadback(FRenderSmokeState& State);
	void ReleaseReadyRetiredSparseActiveBrickCountReadbacks();
	void ProcessSmokeTestProbeRequests(FRDGBuilder& GraphBuilder, uint64 SceneKey);
	void ConsumeSmokeTestProbeReadbacks();
	void ConsumeVortexMaskValidationReadbacks();
	bool HasRenderableSceneState_RenderThread(uint64 SceneKey, const FSceneViewFamily* ViewFamily = nullptr) const;
	void UploadDeadVortexParticles(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGBufferRef VortexBuffer);
	void AddVortexParticleUpdatePass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGBufferRef VortexIn, FRDGBufferRef VortexOut, FRDGTextureRef DensityIn, FRDGTextureRef DisplacedDensityIn, FRDGTextureRef VelocityIn, FRDGTextureRef BulletCutoutTexture, FRDGTextureRef BulletSinkTexture, FRDGBufferRef EventBuffer, int32 EventCount, float DeltaSeconds);
	FRDGBufferRef AddBuildVortexBrickMasksPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGBufferRef VortexBuffer);
	void AddVortexParticleSplatPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGBufferRef VortexBuffer, FRDGBufferRef VortexBrickMasksBuffer, FRDGTextureRef DensityIn, FRDGTextureRef DisplacedDensityIn, FRDGTextureRef VelocityIn, FRDGTextureRef BulletCutoutTexture, FRDGTextureRef BulletSinkTexture, FRDGTextureRef VelocityOut);
	void AddBuildEventBrickMasksPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGBufferRef AdvectionEventBuffer, int32 AdvectionEventCount, FRDGBufferRef ExplosionEventBuffer, int32 ExplosionEventCount, FRDGBufferRef ActorEventBuffer, int32 ActorEventCount, FRDGBufferRef VortexEventBuffer, int32 VortexEventCount, FRDGBufferRef& EmptyEventBuffer, FRDGBufferRef& EmptyEventBrickMasksBuffer, FRDGBufferRef& OutAdvectionEventBrickMasksBuffer, FRDGBufferRef& OutExplosionEventBrickMasksBuffer, FRDGBufferRef& OutActorEventBrickMasksBuffer, FRDGBufferRef& OutVortexEventBrickMasksBuffer);
	void AddBuildBrickOccupancyPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef DensityTexture, FRDGTextureRef DisplacedDensityTexture, FRDGTextureRef VelocityTexture, FRDGTextureRef BulletCutoutTexture, FRDGTextureRef BulletSinkTexture, FRDGBufferRef EventBuffer, int32 EventCount, FRDGTextureRef BrickActivityTexture, bool bCheckBulletChannels);
	FActiveBrickDispatchResources AddExpandBrickOccupancyPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef BrickActivityTexture, FRDGTextureRef BrickOccupancyTexture, uint32 MaxActiveListBricks, uint32 MaxDispatchBrickCount);
	FActiveBrickDispatchResources AddBuildActiveBrickListPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef BrickOccupancyTexture);
	FRDGBufferRef AddBuildSparseBrickDispatchArgsPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, const FActiveBrickDispatchResources& ActiveBrickResources, uint32 GroupsPerBrick, uint32 MaxDispatchBrickCount);
	void AddScatterSparseAtlasPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef DensityTexture, FRDGTextureRef DisplacedDensityTexture, FRDGTextureRef BulletCutoutTexture, FRDGTextureRef BulletSinkTexture, const FActiveBrickDispatchResources& ActiveBrickResources, bool bScatterBulletChannels);
	void AddPackDenseFieldPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef DensityTexture, FRDGTextureRef DisplacedDensityTexture, FRDGTextureRef BulletCutoutTexture, FRDGTextureRef BulletSinkTexture, bool bPackBulletChannels);
	void SimulateSmoke(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, float DeltaSeconds);
	void AddInitPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef DensityTexture, FRDGTextureRef DisplacedDensityTexture, FRDGTextureRef VelocityTexture);
	void AddApplyEventsPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef DensityIn, FRDGTextureRef DisplacedDensityIn, FRDGTextureRef VelocityIn, FRDGTextureRef DensityOut, FRDGTextureRef DisplacedDensityOut, FRDGTextureRef VelocityOut, FRDGBufferRef EventBuffer, FRDGBufferRef EventBrickMasksBuffer, int32 EventCount, float DeltaSeconds, const FActiveBrickDispatchResources* ActiveBrickResources);
	void AddDynamicObstaclePass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef DensityIn, FRDGTextureRef DisplacedDensityIn, FRDGTextureRef VelocityIn, FRDGTextureRef DensityOut, FRDGTextureRef DisplacedDensityOut, FRDGTextureRef VelocityOut, FRDGBufferRef EventBuffer, FRDGBufferRef EventBrickMasksBuffer, int32 EventCount, float DeltaSeconds, const FActiveBrickDispatchResources* ActiveBrickResources);
	void AddSimulatePass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef DensityIn, FRDGTextureRef DisplacedDensityIn, FRDGTextureRef VelocityIn, FRDGTextureRef BulletCutoutTexture, FRDGTextureRef BulletSinkTexture, FRDGBufferRef EventBuffer, FRDGBufferRef EventBrickMasksBuffer, int32 EventCount, FRDGTextureRef DensityOut, FRDGTextureRef DisplacedDensityOut, FRDGTextureRef VelocityOut, FRDGTextureRef BulletCutoutOut, FRDGTextureRef BulletSinkOut, float DeltaSeconds, const FActiveBrickDispatchResources* ActiveBrickResources);
	void AddBuildCurlPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef VelocityIn, FRDGTextureRef CurlOut);
	void AddVorticityPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef DensityIn, FRDGTextureRef DisplacedDensityIn, FRDGTextureRef VelocityIn, FRDGTextureRef CurlTexture, FRDGTextureRef VelocityOut, FRDGBufferRef EventBuffer, FRDGBufferRef EventBrickMasksBuffer, int32 EventCount, float DeltaSeconds);
	void AddDivergencePass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef VelocityIn, FRDGTextureRef DivergenceOut, FRDGTextureRef PressureOut);
	void AddBuildMacDivergencePass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef VelocityIn, FRDGTextureRef FaceVelocityUOut, FRDGTextureRef FaceVelocityVOut, FRDGTextureRef FaceVelocityWOut, FRDGTextureRef DivergenceOut, FRDGTextureRef PressureOut, const FActiveBrickDispatchResources* ActiveBrickResources);
	void AddPressureJacobiPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, const FIntVector& GridSize, const FVector3f& CellSize, FRDGTextureRef PressureIn, FRDGTextureRef DivergenceIn, FRDGTextureRef PressureOut, const FActiveBrickDispatchResources* ActiveBrickResources, int32 IterationIndex);
	void AddProjectVelocityPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef VelocityIn, FRDGTextureRef PressureIn, FRDGTextureRef VelocityOut);
	void AddProjectMacToCollocatedVelocityPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef FaceVelocityUIn, FRDGTextureRef FaceVelocityVIn, FRDGTextureRef FaceVelocityWIn, FRDGTextureRef PressureIn, FRDGTextureRef VelocityOut, const FActiveBrickDispatchResources* ActiveBrickResources);

	TMap<FRenderSmokeStateKey, FRenderSmokeState> SmokeStates;
	TMap<uint64, float> LastFrameDeltaSecondsByScene;
	TArray<FRetiredSparseActiveBrickCountReadback> RetiredSparseActiveBrickCountReadbacks;
	TArray<FPendingSmokeTestProbeReadback> PendingSmokeTestProbeReadbacks;
	TArray<FPendingVortexMaskValidationReadback> PendingVortexMaskValidationReadbacks;
	FTimeThiefSmokeTestGpuProfiler SmokeTestGpuProfiler;
};
