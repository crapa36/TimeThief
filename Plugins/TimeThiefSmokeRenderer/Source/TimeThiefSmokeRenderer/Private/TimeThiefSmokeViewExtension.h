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
	struct FRenderSmokeState
	{
		struct FCarrierParticle
		{
			FVector3f LocalPosition = FVector3f::ZeroVector;
			FVector3f Velocity = FVector3f::ZeroVector;
			float Radius = 0.0f;
			float Phase = 0.0f;
		};

		FTimeThiefSmokeRendererVolume Volume;
		TArray<FTimeThiefSmokeRendererEvent> PendingEvents;
		TArray<FTimeThiefSmokeRendererEvent> LastDebugEvents;
		TArray<FTimeThiefSmokeRendererEvent> AnalyticBulletEvents;
		TArray<FCarrierParticle> CarrierParticles;
		TRefCountPtr<IPooledRenderTarget> DensityTextures[2];
		TRefCountPtr<IPooledRenderTarget> VelocityTextures[2];
		TRefCountPtr<IPooledRenderTarget> MacVelocityUTextures[2];
		TRefCountPtr<IPooledRenderTarget> MacVelocityVTextures[2];
		TRefCountPtr<IPooledRenderTarget> MacVelocityWTextures[2];
		TRefCountPtr<IPooledRenderTarget> PressureTextures[2];
		TRefCountPtr<IPooledRenderTarget> DivergenceTexture;
		TRefCountPtr<IPooledRenderTarget> BulletCutoutTextures[2];
		TRefCountPtr<IPooledRenderTarget> BulletSinkTextures[2];
		TRefCountPtr<IPooledRenderTarget> WarpTextures[2];
		TRefCountPtr<IPooledRenderTarget> ObstacleTexture;
		TRefCountPtr<IPooledRenderTarget> BrickOccupancyTexture;
		TRefCountPtr<IPooledRenderTarget> SparseFieldAtlasTexture;
		TRefCountPtr<IPooledRenderTarget> CarrierFieldTexture;
		TRefCountPtr<IPooledRenderTarget> CurlTexture;
		TRefCountPtr<FRDGPooledBuffer> CarrierParticleBuffers[2];
		TRefCountPtr<FRDGPooledBuffer> VortexParticleBuffers[2];
		TSharedPtr<FRHIGPUBufferReadback> ActiveBrickCountReadback;
		TArray<uint8> ObstacleUploadScratch;
		TArray<uint8> ObstaclePreviousScratch;
		TArray<uint8> ObstacleTargetScratch;
		int32 CurrentDensityIndex = 0;
		int32 CurrentVelocityIndex = 0;
		int32 CurrentBulletFieldIndex = 0;
		int32 CurrentWarpIndex = 0;
		int32 CurrentCarrierParticleIndex = 0;
		int32 CurrentVortexParticleIndex = 0;
		FIntVector AllocatedGridSize = FIntVector::ZeroValue;
		FIntVector AllocatedCarrierFieldGridSize = FIntVector::ZeroValue;
		FIntVector AllocatedBrickGridSize = FIntVector::ZeroValue;
		FIntVector AllocatedSparseAtlasBrickGridSize = FIntVector::ZeroValue;
		FIntVector AllocatedSparseAtlasGridSize = FIntVector::ZeroValue;
		int32 EffectiveSmokeGridResolution = 0;
		int32 AllocatedObstacleResolution = 0;
		uint32 LastActiveBrickCount = 0;
		uint32 UploadedObstacleMaskRevision = MAX_uint32;
		uint32 TargetObstacleMaskRevision = MAX_uint32;
		float ObstacleMaskBlendAge = 1.0f;
		uint32 LastSimulatedFrame = MAX_uint32;
		int32 AllocatedCarrierParticleCount = 0;
		int32 AllocatedVortexParticleCount = 0;
		int32 LastProfilePassCount = 0;
		uint32 LastProfileLogFrame = 0;
		uint64 LastEstimatedVRAMBytes = 0;
		float AccumulatedSimulationDeltaSeconds = 0.0f;
		float AccumulatedCarrierDeltaSeconds = 0.0f;
		float AccumulatedVortexDeltaSeconds = 0.0f;
		float WarpDecayBudgetSeconds = 0.0f;
		bool bNeedsInit = true;
		bool bCarrierParticlesNeedUpload = true;
		bool bVortexParticlesNeedUpload = true;
		bool bCarrierFieldNeedsRefresh = true;
		bool bWarnedBrickBudgetOverflow = false;
		bool bActiveBrickCountReadbackPending = false;
		bool bForceDenseComposite = false;
		bool bWarpClearPending = false;
		bool bUseSparseSimulationMaskThisFrame = false;
	};

	FScreenPassTexture CompositeSmokeMulti_RenderThread(
		FRDGBuilder& GraphBuilder,
		const FSceneView& View,
		const FPostProcessMaterialInputs& Inputs,
		const TArray<FRenderSmokeState*>& RenderStates,
		FScreenPassTexture CurrentSceneColor,
		const FMatrix44f& InvViewProjection,
		bool bAllowOverrideOutput);

	void EnsureResources(FRDGBuilder& GraphBuilder, FRenderSmokeState& State);
	void EnsureObstacleTexture(FRDGBuilder& GraphBuilder, FRenderSmokeState& State);
	void EnsureCarrierParticles(FRenderSmokeState& State);
	void EnsureCarrierParticleBuffers(FRDGBuilder& GraphBuilder, FRenderSmokeState& State);
	void UploadCarrierParticles(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGBufferRef CarrierBuffer);
	void EnsureVortexParticleBuffers(FRDGBuilder& GraphBuilder, FRenderSmokeState& State);
	void UploadDeadVortexParticles(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGBufferRef VortexBuffer);
	void AddCarrierParticleUpdatePass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGBufferRef CarrierIn, FRDGBufferRef CarrierOut, FRDGBufferRef EventBuffer, int32 EventCount, float DeltaSeconds);
	void AddBuildCarrierFieldPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGBufferRef CarrierBuffer, FRDGTextureRef CarrierFieldTexture);
	void AddVortexParticleUpdatePass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGBufferRef VortexIn, FRDGBufferRef VortexOut, FRDGTextureRef DensityIn, FRDGTextureRef VelocityIn, FRDGTextureRef BulletCutoutTexture, FRDGTextureRef BulletSinkTexture, FRDGBufferRef EventBuffer, int32 EventCount, float DeltaSeconds);
	FRDGBufferRef AddBuildVortexBrickMasksPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGBufferRef VortexBuffer);
	void AddVortexParticleSplatPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGBufferRef VortexBuffer, FRDGBufferRef VortexBrickMasksBuffer, FRDGTextureRef DensityIn, FRDGTextureRef VelocityIn, FRDGTextureRef BulletCutoutTexture, FRDGTextureRef BulletSinkTexture, FRDGTextureRef VelocityOut);
	FRDGBufferRef AddBuildEventBrickMasksPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGBufferRef EventBuffer, int32 EventCount, const TCHAR* DebugName);
	void AddBuildBrickOccupancyPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef DensityTexture, FRDGTextureRef VelocityTexture, FRDGTextureRef WarpTexture, FRDGTextureRef BulletCutoutTexture, FRDGTextureRef BulletSinkTexture, FRDGBufferRef ActorEventBuffer, int32 ActorEventCount, FRDGTextureRef BrickActivityTexture);
	void AddExpandBrickOccupancyPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef BrickActivityTexture, FRDGTextureRef BrickOccupancyTexture);
	void AddScatterSparseAtlasPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef DensityTexture, FRDGTextureRef WarpTexture, FRDGTextureRef BulletCutoutTexture, FRDGTextureRef BulletSinkTexture, FRDGTextureRef BrickOccupancyTexture);
	void SimulateSmoke(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, float DeltaSeconds);
	void AddInitPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef DensityTexture, FRDGTextureRef VelocityTexture);
	void AddApplyEventsPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef DensityIn, FRDGTextureRef VelocityIn, FRDGTextureRef DensityOut, FRDGTextureRef VelocityOut, FRDGBufferRef EventBuffer, FRDGBufferRef EventBrickMasksBuffer, int32 EventCount, float DeltaSeconds);
	void AddDynamicObstaclePass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef DensityIn, FRDGTextureRef VelocityIn, FRDGTextureRef DensityOut, FRDGTextureRef VelocityOut, FRDGBufferRef EventBuffer, FRDGBufferRef EventBrickMasksBuffer, int32 EventCount, float DeltaSeconds);
	void AddSimulatePass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef DensityIn, FRDGTextureRef VelocityIn, FRDGTextureRef BulletCutoutTexture, FRDGTextureRef BulletSinkTexture, FRDGBufferRef CarrierBuffer, FRDGTextureRef CarrierFieldTexture, FRDGBufferRef EventBuffer, FRDGBufferRef EventBrickMasksBuffer, int32 EventCount, FRDGTextureRef DensityOut, FRDGTextureRef VelocityOut, FRDGTextureRef BulletCutoutOut, FRDGTextureRef BulletSinkOut, float DeltaSeconds);
	void AddWarpPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef DensityIn, FRDGTextureRef WarpIn, FRDGTextureRef WarpOut, FRDGBufferRef EventBuffer, FRDGBufferRef EventBrickMasksBuffer, int32 EventCount, float DeltaSeconds);
	void AddBuildCurlPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef VelocityIn, FRDGTextureRef CurlOut);
	void AddVorticityPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef DensityIn, FRDGTextureRef VelocityIn, FRDGTextureRef CurlTexture, FRDGTextureRef VelocityOut, FRDGBufferRef EventBuffer, FRDGBufferRef EventBrickMasksBuffer, int32 EventCount, float DeltaSeconds);
	void AddDivergencePass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef VelocityIn, FRDGTextureRef DivergenceOut, FRDGTextureRef PressureOut);
	void AddBuildMacVelocityPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef VelocityIn, FRDGTextureRef FaceVelocityUOut, FRDGTextureRef FaceVelocityVOut, FRDGTextureRef FaceVelocityWOut);
	void AddMacDivergencePass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef FaceVelocityUIn, FRDGTextureRef FaceVelocityVIn, FRDGTextureRef FaceVelocityWIn, FRDGTextureRef DivergenceOut, FRDGTextureRef PressureOut);
	FRDGTextureRef AddPressureSolvePass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef DivergenceIn, FRDGTextureRef ObstacleTexture, FRDGTextureRef PressureA, FRDGTextureRef PressureB);
	void AddPressureJacobiPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, const FIntVector& GridSize, const FVector3f& CellSize, FRDGTextureRef PressureIn, FRDGTextureRef DivergenceIn, FRDGTextureRef ObstacleTexture, FRDGTextureRef PressureOut);
	void AddPressureResidualPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, const FIntVector& GridSize, const FVector3f& CellSize, FRDGTextureRef PressureIn, FRDGTextureRef DivergenceIn, FRDGTextureRef ObstacleTexture, FRDGTextureRef ResidualOut);
	void AddPressureRestrictPass(FRDGBuilder& GraphBuilder, const FIntVector& FineGridSize, const FIntVector& CoarseGridSize, FRDGTextureRef FineDivergenceIn, FRDGTextureRef CoarseDivergenceOut);
	void AddPressureProlongateAddPass(FRDGBuilder& GraphBuilder, const FIntVector& FineGridSize, const FIntVector& CoarseGridSize, FRDGTextureRef FinePressureIn, FRDGTextureRef CoarsePressureIn, FRDGTextureRef FinePressureOut);
	void AddProjectVelocityPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef VelocityIn, FRDGTextureRef PressureIn, FRDGTextureRef VelocityOut);
	void AddProjectMacVelocityPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef FaceVelocityUIn, FRDGTextureRef FaceVelocityVIn, FRDGTextureRef FaceVelocityWIn, FRDGTextureRef PressureIn, FRDGTextureRef FaceVelocityUOut, FRDGTextureRef FaceVelocityVOut, FRDGTextureRef FaceVelocityWOut);
	void AddMacToCollocatedVelocityPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef FaceVelocityUIn, FRDGTextureRef FaceVelocityVIn, FRDGTextureRef FaceVelocityWIn, FRDGTextureRef VelocityOut);

	TMap<int32, FRenderSmokeState> SmokeStates;
	float LastFrameDeltaSeconds = 0.0f;
};
