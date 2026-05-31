#pragma once

#include "CoreMinimal.h"
#include "RHIGPUReadback.h"
#include "SceneViewExtension.h"
#include "TimeThiefSmokeRendererTypes.h"

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
		TRefCountPtr<IPooledRenderTarget> BrickOccupancyTexture;
		TRefCountPtr<IPooledRenderTarget> SparseFieldAtlasTexture;
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
		uint32 LastSimulatedFrame = MAX_uint32;
		int32 AllocatedVortexParticleCount = 0;
		float AccumulatedSimulationDeltaSeconds = 0.0f;
		float AccumulatedVortexDeltaSeconds = 0.0f;
		float BulletFieldDecayBudgetSeconds = 0.0f;
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

	FScreenPassTexture CompositeSmokeMulti_RenderThread(
		FRDGBuilder& GraphBuilder,
		const FSceneView& View,
		const FPostProcessMaterialInputs& Inputs,
		const TArray<FRenderSmokeState*>& RenderStates,
		const TArray<FIntRect>& RenderRects,
		FScreenPassTexture CurrentSceneColor,
		const FMatrix44f& InvViewProjection,
		bool bAllowOverrideOutput);

	void EnsureResources(FRDGBuilder& GraphBuilder, FRenderSmokeState& State);
	void EnsureObstacleFieldTextures(FRDGBuilder& GraphBuilder, FRenderSmokeState& State);
	void EnsureVortexParticleBuffers(FRDGBuilder& GraphBuilder, FRenderSmokeState& State);
	void ConsumeSparseActiveBrickCountReadback(FRenderSmokeState& State);
	void QueueSparseActiveBrickCountReadback(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGBufferRef ActiveBrickCountBuffer);
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
	void SimulateSmoke(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, float DeltaSeconds);
	void AddInitPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef DensityTexture, FRDGTextureRef DisplacedDensityTexture, FRDGTextureRef VelocityTexture);
	void AddApplyEventsPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef DensityIn, FRDGTextureRef DisplacedDensityIn, FRDGTextureRef VelocityIn, FRDGTextureRef DensityOut, FRDGTextureRef DisplacedDensityOut, FRDGTextureRef VelocityOut, FRDGBufferRef EventBuffer, FRDGBufferRef EventBrickMasksBuffer, int32 EventCount, float DeltaSeconds, const FActiveBrickDispatchResources* ActiveBrickResources);
	void AddDynamicObstaclePass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef DensityIn, FRDGTextureRef DisplacedDensityIn, FRDGTextureRef VelocityIn, FRDGTextureRef DensityOut, FRDGTextureRef DisplacedDensityOut, FRDGTextureRef VelocityOut, FRDGBufferRef EventBuffer, FRDGBufferRef EventBrickMasksBuffer, int32 EventCount, float DeltaSeconds, const FActiveBrickDispatchResources* ActiveBrickResources);
	void AddSimulatePass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef DensityIn, FRDGTextureRef DisplacedDensityIn, FRDGTextureRef VelocityIn, FRDGTextureRef BulletCutoutTexture, FRDGTextureRef BulletSinkTexture, FRDGBufferRef EventBuffer, FRDGBufferRef EventBrickMasksBuffer, int32 EventCount, FRDGTextureRef DensityOut, FRDGTextureRef DisplacedDensityOut, FRDGTextureRef VelocityOut, FRDGTextureRef BulletCutoutOut, FRDGTextureRef BulletSinkOut, float DeltaSeconds, const FActiveBrickDispatchResources* ActiveBrickResources);
	void AddBuildCurlPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef VelocityIn, FRDGTextureRef CurlOut);
	void AddVorticityPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef DensityIn, FRDGTextureRef DisplacedDensityIn, FRDGTextureRef VelocityIn, FRDGTextureRef CurlTexture, FRDGTextureRef VelocityOut, FRDGBufferRef EventBuffer, FRDGBufferRef EventBrickMasksBuffer, int32 EventCount, float DeltaSeconds);
	void AddDivergencePass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef VelocityIn, FRDGTextureRef DivergenceOut, FRDGTextureRef PressureOut);
	void AddBuildMacDivergencePass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef VelocityIn, FRDGTextureRef FaceVelocityUOut, FRDGTextureRef FaceVelocityVOut, FRDGTextureRef FaceVelocityWOut, FRDGTextureRef DivergenceOut, FRDGTextureRef PressureOut, const FActiveBrickDispatchResources* ActiveBrickResources);
	void AddPressureJacobiPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, const FIntVector& GridSize, const FVector3f& CellSize, FRDGTextureRef PressureIn, FRDGTextureRef DivergenceIn, FRDGTextureRef ObstacleTexture, FRDGTextureRef PressureOut, const FActiveBrickDispatchResources* ActiveBrickResources);
	void AddProjectVelocityPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef VelocityIn, FRDGTextureRef PressureIn, FRDGTextureRef VelocityOut);
	void AddProjectMacToCollocatedVelocityPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef FaceVelocityUIn, FRDGTextureRef FaceVelocityVIn, FRDGTextureRef FaceVelocityWIn, FRDGTextureRef PressureIn, FRDGTextureRef VelocityOut, const FActiveBrickDispatchResources* ActiveBrickResources);

	TMap<FRenderSmokeStateKey, FRenderSmokeState> SmokeStates;
	TMap<uint64, float> LastFrameDeltaSecondsByScene;
};
