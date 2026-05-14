#pragma once

#include "CoreMinimal.h"
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
		TArray<FCarrierParticle> CarrierParticles;
		TRefCountPtr<IPooledRenderTarget> DensityTextures[2];
		TRefCountPtr<IPooledRenderTarget> VelocityTextures[2];
		TRefCountPtr<IPooledRenderTarget> PressureTextures[2];
		TRefCountPtr<IPooledRenderTarget> DivergenceTexture;
		TRefCountPtr<IPooledRenderTarget> BulletSuppressionTextures[2];
		TRefCountPtr<IPooledRenderTarget> WarpTextures[2];
		TRefCountPtr<IPooledRenderTarget> ObstacleTexture;
		TRefCountPtr<FRDGPooledBuffer> CarrierParticleBuffers[2];
		TArray<uint8> ObstacleUploadScratch;
		TArray<uint8> ObstaclePreviousScratch;
		TArray<uint8> ObstacleTargetScratch;
		int32 CurrentDensityIndex = 0;
		int32 CurrentVelocityIndex = 0;
		int32 CurrentBulletSuppressionIndex = 0;
		int32 CurrentWarpIndex = 0;
		int32 CurrentCarrierParticleIndex = 0;
		int32 AllocatedResolution = 0;
		int32 AllocatedObstacleResolution = 0;
		uint32 UploadedObstacleMaskRevision = MAX_uint32;
		uint32 TargetObstacleMaskRevision = MAX_uint32;
		float ObstacleMaskBlendAge = 1.0f;
		uint32 LastSimulatedFrame = MAX_uint32;
		int32 AllocatedCarrierParticleCount = 0;
		bool bNeedsInit = true;
		bool bCarrierParticlesNeedUpload = true;
	};

	void EnsureResources(FRDGBuilder& GraphBuilder, FRenderSmokeState& State);
	void EnsureObstacleTexture(FRDGBuilder& GraphBuilder, FRenderSmokeState& State);
	void EnsureCarrierParticles(FRenderSmokeState& State);
	void EnsureCarrierParticleBuffers(FRDGBuilder& GraphBuilder, FRenderSmokeState& State);
	void UploadCarrierParticles(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGBufferRef CarrierBuffer);
	void AddCarrierParticleUpdatePass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGBufferRef CarrierIn, FRDGBufferRef CarrierOut, const TArray<FTimeThiefSmokeRendererEvent>& Events, float DeltaSeconds);
	void SimulateSmoke(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, float DeltaSeconds);
	void AddInitPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef DensityTexture, FRDGTextureRef VelocityTexture);
	void AddApplyEventsPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef DensityIn, FRDGTextureRef VelocityIn, FRDGTextureRef DensityOut, FRDGTextureRef VelocityOut, const TArray<FTimeThiefSmokeRendererEvent>& Events, float DeltaSeconds);
	void AddBulletSuppressionPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef SuppressionIn, FRDGTextureRef SuppressionOut, const TArray<FTimeThiefSmokeRendererEvent>& Events, float DeltaSeconds);
	void AddDynamicObstaclePass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef DensityIn, FRDGTextureRef VelocityIn, FRDGTextureRef DensityOut, FRDGTextureRef VelocityOut, const TArray<FTimeThiefSmokeRendererEvent>& Events);
	void AddSimulatePass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef DensityIn, FRDGTextureRef VelocityIn, FRDGTextureRef BulletSuppressionTexture, FRDGBufferRef CarrierBuffer, FRDGTextureRef DensityOut, FRDGTextureRef VelocityOut, float DeltaSeconds);
	void AddWarpPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef DensityIn, FRDGTextureRef WarpIn, FRDGTextureRef WarpOut, const TArray<FTimeThiefSmokeRendererEvent>& Events, float DeltaSeconds);
	void AddVorticityPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef DensityIn, FRDGTextureRef VelocityIn, FRDGTextureRef VelocityOut, const TArray<FTimeThiefSmokeRendererEvent>& Events, float DeltaSeconds);
	void AddDivergencePass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef VelocityIn, FRDGTextureRef DivergenceOut, FRDGTextureRef PressureOut);
	void AddPressureJacobiPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef PressureIn, FRDGTextureRef DivergenceIn, FRDGTextureRef ObstacleTexture, FRDGTextureRef PressureOut);
	void AddProjectVelocityPass(FRDGBuilder& GraphBuilder, FRenderSmokeState& State, FRDGTextureRef VelocityIn, FRDGTextureRef PressureIn, FRDGTextureRef VelocityOut);

	TMap<int32, FRenderSmokeState> SmokeStates;
	float LastFrameDeltaSeconds = 0.0f;
};
