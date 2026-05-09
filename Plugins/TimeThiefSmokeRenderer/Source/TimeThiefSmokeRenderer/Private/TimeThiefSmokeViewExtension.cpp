#include "TimeThiefSmokeViewExtension.h"

#include "PixelShaderUtils.h"
#include "HAL/IConsoleManager.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"
#include "RHITypes.h"
#include "SceneView.h"
#include "ScreenPass.h"
#include "TimeThiefSmokeShaders.h"

namespace
{
	constexpr uint32 SmokeThreadGroupSize = 4;
	constexpr int32 MaxCarrierParticleCount = 128;
	constexpr int32 MaxDebugEventCount = 128;
	constexpr float ObstacleMaskBlendDuration = 0.25f;

	static TAutoConsoleVariable<int32> CVarTimeThiefSmokeDebugView(
		TEXT("r.TimeThiefSmoke.DebugView"),
		0,
		TEXT("Custom smoke debug view. 0=off, 1=density, 2=obstacle, 3=bullet suppression, 4=carrier particles, 5=events."));

	static TAutoConsoleVariable<int32> CVarTimeThiefSmokeScissor(
		TEXT("r.TimeThiefSmoke.Scissor"),
		1,
		TEXT("Limits custom smoke composite draw calls to each smoke AABB screen rect when enabled."));

	FIntVector MakeGridSize(const int32 Resolution)
	{
		const int32 ClampedResolution = FMath::Clamp(Resolution, 16, 128);
		return FIntVector(ClampedResolution, ClampedResolution, ClampedResolution);
	}

	FIntVector MakeGroupCount(const FIntVector& GridSize)
	{
		return FIntVector(
			FMath::DivideAndRoundUp(GridSize.X, static_cast<int32>(SmokeThreadGroupSize)),
			FMath::DivideAndRoundUp(GridSize.Y, static_cast<int32>(SmokeThreadGroupSize)),
			FMath::DivideAndRoundUp(GridSize.Z, static_cast<int32>(SmokeThreadGroupSize)));
	}

	FTimeThiefSmokeEventShaderData ToShaderEvent(const FTimeThiefSmokeRendererEvent& Event)
	{
		FTimeThiefSmokeEventShaderData ShaderEvent;
		const FVector3f Position(Event.Position);
		const FVector3f Direction = FVector3f(Event.Direction.GetSafeNormal());
		const FVector3f Extents(Event.Extents);

		ShaderEvent.PositionRadius = FVector4f(Position.X, Position.Y, Position.Z, Event.Radius);
		ShaderEvent.DirectionLength = FVector4f(Direction.X, Direction.Y, Direction.Z, Event.Length);
		ShaderEvent.ExtentsStrength = FVector4f(Extents.X, Extents.Y, Extents.Z, Event.Strength);
		ShaderEvent.Rotation = FVector4f(
			static_cast<float>(Event.Rotation.X),
			static_cast<float>(Event.Rotation.Y),
			static_cast<float>(Event.Rotation.Z),
			static_cast<float>(Event.Rotation.W));
		ShaderEvent.TypeShapeAgeSeed = FVector4f(
			static_cast<float>(Event.Type),
			static_cast<float>(Event.Shape),
			Event.NormalizedAge,
			static_cast<float>(Event.Seed));
		return ShaderEvent;
	}

	FTransform ToDoubleTransform(const FTransform3f& Transform)
	{
		return FTransform(
			FQuat(Transform.GetRotation()),
			FVector(Transform.GetTranslation()),
			FVector(Transform.GetScale3D()));
	}

	bool HasEventType(const TArray<FTimeThiefSmokeRendererEvent>& Events, ETimeThiefSmokeRendererInteractionType Type)
	{
		for (const FTimeThiefSmokeRendererEvent& Event : Events)
		{
			if (Event.Type == Type)
			{
				return true;
			}
		}
		return false;
	}

	bool ComputeSmokeScreenRect(
		const FSceneView& View,
		const FTimeThiefSmokeRendererVolume& Volume,
		const FIntRect& ViewRect,
		FIntRect& OutRect)
	{
		const FVector BoundsExtent(Volume.BoundsExtent);
		const FTransform LocalToWorld = ToDoubleTransform(Volume.LocalToWorld);
		const FMatrix ViewProjection = View.ViewMatrices.GetViewProjectionMatrix();
		bool bCrossesNearPlane = false;
		bool bHasProjectedCorner = false;
		float MinX = TNumericLimits<float>::Max();
		float MinY = TNumericLimits<float>::Max();
		float MaxX = -TNumericLimits<float>::Max();
		float MaxY = -TNumericLimits<float>::Max();

		for (int32 XSign = -1; XSign <= 1; XSign += 2)
		{
			for (int32 YSign = -1; YSign <= 1; YSign += 2)
			{
				for (int32 ZSign = -1; ZSign <= 1; ZSign += 2)
				{
					const FVector LocalCorner(
						BoundsExtent.X * static_cast<double>(XSign),
						BoundsExtent.Y * static_cast<double>(YSign),
						BoundsExtent.Z * static_cast<double>(ZSign));
					const FVector WorldCorner = LocalToWorld.TransformPosition(LocalCorner);
					const FVector4 Clip = ViewProjection.TransformFVector4(FVector4(WorldCorner, 1.0));
					if (Clip.W <= UE_SMALL_NUMBER)
					{
						bCrossesNearPlane = true;
						continue;
					}

					const float InvW = 1.0f / static_cast<float>(Clip.W);
					const float NdcX = static_cast<float>(Clip.X) * InvW;
					const float NdcY = static_cast<float>(Clip.Y) * InvW;
					const float ScreenX = static_cast<float>(ViewRect.Min.X) + (NdcX * 0.5f + 0.5f) * static_cast<float>(ViewRect.Width());
					const float ScreenY = static_cast<float>(ViewRect.Min.Y) + (0.5f - NdcY * 0.5f) * static_cast<float>(ViewRect.Height());
					MinX = FMath::Min(MinX, ScreenX);
					MinY = FMath::Min(MinY, ScreenY);
					MaxX = FMath::Max(MaxX, ScreenX);
					MaxY = FMath::Max(MaxY, ScreenY);
					bHasProjectedCorner = true;
				}
			}
		}

		if (bCrossesNearPlane)
		{
			OutRect = ViewRect;
			return true;
		}

		if (!bHasProjectedCorner)
		{
			return false;
		}

		constexpr int32 Padding = 12;
		const int32 RectMinX = FMath::Clamp(FMath::FloorToInt(MinX) - Padding, ViewRect.Min.X, ViewRect.Max.X);
		const int32 RectMinY = FMath::Clamp(FMath::FloorToInt(MinY) - Padding, ViewRect.Min.Y, ViewRect.Max.Y);
		const int32 RectMaxX = FMath::Clamp(FMath::CeilToInt(MaxX) + Padding, ViewRect.Min.X, ViewRect.Max.X);
		const int32 RectMaxY = FMath::Clamp(FMath::CeilToInt(MaxY) + Padding, ViewRect.Min.Y, ViewRect.Max.Y);
		OutRect = FIntRect(RectMinX, RectMinY, RectMaxX, RectMaxY);
		return OutRect.Width() > 0 && OutRect.Height() > 0;
	}
}

FTimeThiefSmokeViewExtension::FTimeThiefSmokeViewExtension(const FAutoRegister& AutoRegister)
	: FSceneViewExtensionBase(AutoRegister)
{
}

void FTimeThiefSmokeViewExtension::SubmitFrame_RenderThread(FTimeThiefSmokeRendererFrame&& Frame)
{
	check(IsInRenderingThread());

	LastFrameDeltaSeconds = FMath::Max(Frame.DeltaSeconds, 1.0f / 120.0f);

	TSet<int32> ActiveSmokeIds;
	for (const FTimeThiefSmokeRendererVolume& Volume : Frame.Volumes)
	{
		if (Volume.SmokeId == INDEX_NONE)
		{
			continue;
		}

		ActiveSmokeIds.Add(Volume.SmokeId);
		FRenderSmokeState& State = SmokeStates.FindOrAdd(Volume.SmokeId);
		const int32 NewResolution = FMath::Clamp(Volume.Settings.SmokeGridResolution, 16, 128);
		if (State.AllocatedResolution != NewResolution)
		{
			State.AllocatedResolution = NewResolution;
			State.DensityTextures[0].SafeRelease();
			State.DensityTextures[1].SafeRelease();
			State.VelocityTextures[0].SafeRelease();
			State.VelocityTextures[1].SafeRelease();
			State.PressureTextures[0].SafeRelease();
			State.PressureTextures[1].SafeRelease();
			State.DivergenceTexture.SafeRelease();
			State.BulletSuppressionTextures[0].SafeRelease();
			State.BulletSuppressionTextures[1].SafeRelease();
			State.ObstacleTexture.SafeRelease();
			State.CarrierParticleBuffers[0].SafeRelease();
			State.CarrierParticleBuffers[1].SafeRelease();
			State.CurrentDensityIndex = 0;
			State.CurrentVelocityIndex = 0;
			State.CurrentBulletSuppressionIndex = 0;
			State.CurrentCarrierParticleIndex = 0;
			State.AllocatedObstacleResolution = 0;
			State.UploadedObstacleMaskRevision = MAX_uint32;
			State.TargetObstacleMaskRevision = MAX_uint32;
			State.ObstacleMaskBlendAge = 1.0f;
			State.AllocatedCarrierParticleCount = 0;
			State.bCarrierParticlesNeedUpload = true;
			State.bNeedsInit = true;
		}
		State.Volume = Volume;
	}

	for (auto It = SmokeStates.CreateIterator(); It; ++It)
	{
		if (!ActiveSmokeIds.Contains(It.Key()))
		{
			It.RemoveCurrent();
		}
		else
		{
			It.Value().PendingEvents.Reset();
			It.Value().LastDebugEvents.Reset();
		}
	}

	for (const FTimeThiefSmokeRendererEvent& Event : Frame.Events)
	{
		if (FRenderSmokeState* State = SmokeStates.Find(Event.SmokeId))
		{
			if (State->PendingEvents.Num() < State->Volume.Settings.MaxGPUEventsPerSmokePerFrame)
			{
				State->PendingEvents.Add(Event);
				if (State->LastDebugEvents.Num() < MaxDebugEventCount)
				{
					State->LastDebugEvents.Add(Event);
				}
			}
		}
	}
}

void FTimeThiefSmokeViewExtension::Clear_RenderThread()
{
	check(IsInRenderingThread());
	SmokeStates.Reset();
}

void FTimeThiefSmokeViewExtension::PreRenderViewFamily_RenderThread(
	FRDGBuilder& GraphBuilder,
	FSceneViewFamily& ViewFamily)
{
	for (TPair<int32, FRenderSmokeState>& SmokePair : SmokeStates)
	{
		SimulateSmoke(GraphBuilder, SmokePair.Value, LastFrameDeltaSeconds);
	}
}

void FTimeThiefSmokeViewExtension::SubscribeToPostProcessingPass(
	EPostProcessingPass Pass,
	const FSceneView& View,
	FAfterPassCallbackDelegateArray& InOutPassCallbacks,
	bool bIsPassEnabled)
{
	if (Pass == EPostProcessingPass::BeforeDOF)
	{
		InOutPassCallbacks.Add(FAfterPassCallbackDelegate::CreateRaw(this, &FTimeThiefSmokeViewExtension::CompositeSmoke_RenderThread));
	}
}

FScreenPassTexture FTimeThiefSmokeViewExtension::CompositeSmoke_RenderThread(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const FPostProcessMaterialInputs& Inputs)
{
	if (SmokeStates.IsEmpty())
	{
		return Inputs.ReturnUntouchedSceneColorForPostProcessing(GraphBuilder);
	}

	const FScreenPassTextureSlice SceneColorSlice = Inputs.GetInput(EPostProcessMaterialInput::SceneColor);
	FScreenPassTexture CurrentSceneColor(SceneColorSlice);
	if (!CurrentSceneColor.IsValid())
	{
		return Inputs.ReturnUntouchedSceneColorForPostProcessing(GraphBuilder);
	}

	const FScreenPassViewInfo ViewInfo(View);
	const FScreenPassTextureViewport InputViewport(CurrentSceneColor);
	const FMatrix44f InvViewProjection(View.ViewMatrices.GetInvViewProjectionMatrix());
	const int32 DebugMode = FMath::Clamp(CVarTimeThiefSmokeDebugView.GetValueOnRenderThread(), 0, 5);
	const bool bUseScissor = CVarTimeThiefSmokeScissor.GetValueOnRenderThread() != 0;

	TArray<FRenderSmokeState*> RenderStates;
	RenderStates.Reserve(SmokeStates.Num());
	for (TPair<int32, FRenderSmokeState>& SmokePair : SmokeStates)
	{
		FRenderSmokeState& State = SmokePair.Value;
		if (State.DensityTextures[State.CurrentDensityIndex].IsValid() &&
			State.CarrierParticleBuffers[State.CurrentCarrierParticleIndex].IsValid())
		{
			RenderStates.Add(&State);
		}
	}

	if (RenderStates.IsEmpty())
	{
		return Inputs.ReturnUntouchedSceneColorForPostProcessing(GraphBuilder);
	}

	for (int32 StateIndex = 0; StateIndex < RenderStates.Num(); ++StateIndex)
	{
		FRenderSmokeState& State = *RenderStates[StateIndex];
		const bool bIsLastSmoke = StateIndex == RenderStates.Num() - 1;
		FIntRect SmokeRect = CurrentSceneColor.ViewRect;
		if (bUseScissor && !ComputeSmokeScreenRect(View, State.Volume, CurrentSceneColor.ViewRect, SmokeRect))
		{
			continue;
		}

		FScreenPassRenderTarget Output = bIsLastSmoke && Inputs.OverrideOutput.IsValid()
			? Inputs.OverrideOutput
			: FScreenPassRenderTarget::CreateFromInput(GraphBuilder, CurrentSceneColor, ERenderTargetLoadAction::ELoad, TEXT("TimeThiefSmoke.Composite"));

		if (Output.Texture != CurrentSceneColor.Texture)
		{
			AddCopyTexturePass(GraphBuilder, CurrentSceneColor.Texture, Output.Texture);
		}

		TArray<FTimeThiefSmokeEventShaderData> ShaderEvents;
		ShaderEvents.Reserve(State.LastDebugEvents.Num());
		for (const FTimeThiefSmokeRendererEvent& Event : State.LastDebugEvents)
		{
			ShaderEvents.Add(ToShaderEvent(Event));
		}
		if (ShaderEvents.IsEmpty())
		{
			ShaderEvents.AddDefaulted();
		}

		FRDGBufferRef EventBuffer = GraphBuilder.CreateBuffer(
			FRDGBufferDesc::CreateStructuredDesc(sizeof(FTimeThiefSmokeEventShaderData), ShaderEvents.Num()),
			TEXT("TimeThiefSmoke.CompositeEvents"));
		GraphBuilder.QueueBufferUpload(EventBuffer, ShaderEvents.GetData(), ShaderEvents.Num() * sizeof(FTimeThiefSmokeEventShaderData));
		FRDGBufferRef CarrierBuffer = GraphBuilder.RegisterExternalBuffer(
			State.CarrierParticleBuffers[State.CurrentCarrierParticleIndex],
			TEXT("TimeThiefSmoke.CompositeCarrierParticles"));

		TShaderMapRef<FTimeThiefSmokeCompositePS> PixelShader(GetGlobalShaderMap(View.FeatureLevel));
		FTimeThiefSmokeCompositePS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeCompositePS::FParameters>();
		PassParameters->SceneColorTexture = CurrentSceneColor.Texture;
		PassParameters->SceneDepthTexture = Inputs.SceneTextures.SceneTextures->GetParameters()->SceneDepthTexture;
		PassParameters->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		PassParameters->DensityTexture = GraphBuilder.RegisterExternalTexture(State.DensityTextures[State.CurrentDensityIndex]);
		PassParameters->ObstacleTexture = GraphBuilder.RegisterExternalTexture(State.ObstacleTexture);
		PassParameters->BulletSuppressionTexture = GraphBuilder.RegisterExternalTexture(State.BulletSuppressionTextures[State.CurrentBulletSuppressionIndex]);
		PassParameters->VolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		PassParameters->CarrierParticles = GraphBuilder.CreateSRV(CarrierBuffer);
		PassParameters->Events = GraphBuilder.CreateSRV(EventBuffer);
		PassParameters->SceneColorUVScaleBias = FVector4f(
			1.0f / FMath::Max(1, CurrentSceneColor.Texture->Desc.Extent.X),
			1.0f / FMath::Max(1, CurrentSceneColor.Texture->Desc.Extent.Y),
			0.0f,
			0.0f);
		PassParameters->ViewRect = CurrentSceneColor.ViewRect;
		PassParameters->BoundsExtent = FVector3f(State.Volume.BoundsExtent);
		PassParameters->Extinction = State.Volume.Settings.Extinction;
		PassParameters->ScatteringAlbedo = State.Volume.Settings.ScatteringAlbedo;
		PassParameters->ScatteringAnisotropy = State.Volume.Settings.ScatteringAnisotropy;
		PassParameters->AgeSeconds = State.Volume.AgeSeconds;
		PassParameters->DurationSeconds = State.Volume.DurationSeconds;
		PassParameters->SmokeFadeOutDuration = State.Volume.Settings.SmokeFadeOutDuration;
		PassParameters->RenderStepCount = FMath::Clamp(State.Volume.Settings.RenderStepCount, 16, 128);
		PassParameters->DebugMode = DebugMode;
		PassParameters->CarrierParticleCount = FMath::Min(State.AllocatedCarrierParticleCount, MaxCarrierParticleCount);
		PassParameters->EventCount = FMath::Min(State.LastDebugEvents.Num(), MaxDebugEventCount);
		PassParameters->InvViewProjection = InvViewProjection;
		PassParameters->LocalToWorld = State.Volume.LocalToWorld.ToMatrixWithScale();
		PassParameters->WorldToLocal = State.Volume.LocalToWorld.ToInverseMatrixWithScale();
		PassParameters->RenderTargets[0] = Output.GetRenderTargetBinding();

		AddDrawScreenPass(
			GraphBuilder,
			RDG_EVENT_NAME("TimeThiefSmoke.Composite SmokeId=%d", State.Volume.SmokeId),
			ViewInfo,
			FScreenPassTextureViewport(Output.Texture, SmokeRect),
			InputViewport,
			PixelShader,
			PassParameters);

		CurrentSceneColor = MoveTemp(Output);
	}

	return CurrentSceneColor;
}

void FTimeThiefSmokeViewExtension::EnsureResources(FRDGBuilder& GraphBuilder, FRenderSmokeState& State)
{
	const FIntVector GridSize = MakeGridSize(State.AllocatedResolution);
	const FRDGTextureDesc DensityDesc = FRDGTextureDesc::Create3D(
		GridSize,
		PF_R16F,
		FClearValueBinding::Black,
		TexCreate_ShaderResource | TexCreate_UAV);
	const FRDGTextureDesc VelocityDesc = FRDGTextureDesc::Create3D(
		GridSize,
		PF_FloatRGBA,
		FClearValueBinding::Black,
		TexCreate_ShaderResource | TexCreate_UAV);
	const FRDGTextureDesc ScalarDesc = FRDGTextureDesc::Create3D(
		GridSize,
		PF_R16F,
		FClearValueBinding::Black,
		TexCreate_ShaderResource | TexCreate_UAV);

	for (int32 TextureIndex = 0; TextureIndex < 2; ++TextureIndex)
	{
		if (!State.DensityTextures[TextureIndex].IsValid())
		{
			AllocatePooledTexture(DensityDesc, State.DensityTextures[TextureIndex], TEXT("TimeThiefSmoke.Density"));
			State.bNeedsInit = true;
		}

		if (!State.VelocityTextures[TextureIndex].IsValid())
		{
			AllocatePooledTexture(VelocityDesc, State.VelocityTextures[TextureIndex], TEXT("TimeThiefSmoke.Velocity"));
			State.bNeedsInit = true;
		}

		if (!State.PressureTextures[TextureIndex].IsValid())
		{
			AllocatePooledTexture(ScalarDesc, State.PressureTextures[TextureIndex], TEXT("TimeThiefSmoke.Pressure"));
			State.bNeedsInit = true;
		}

		if (!State.BulletSuppressionTextures[TextureIndex].IsValid())
		{
			AllocatePooledTexture(ScalarDesc, State.BulletSuppressionTextures[TextureIndex], TEXT("TimeThiefSmoke.BulletSuppression"));
			State.bNeedsInit = true;
		}
	}

	if (!State.DivergenceTexture.IsValid())
	{
		AllocatePooledTexture(ScalarDesc, State.DivergenceTexture, TEXT("TimeThiefSmoke.Divergence"));
		State.bNeedsInit = true;
	}

	EnsureObstacleTexture(GraphBuilder, State);
}

void FTimeThiefSmokeViewExtension::EnsureObstacleTexture(FRDGBuilder& GraphBuilder, FRenderSmokeState& State)
{
	const int32 SourceResolution = State.Volume.ObstacleMaskResolution;
	const int32 SourceVoxelCount = SourceResolution > 0 ? SourceResolution * SourceResolution * SourceResolution : 0;
	const bool bHasValidMask = SourceResolution > 0 && State.Volume.ObstacleMask.Num() == SourceVoxelCount;
	const int32 DesiredResolution = bHasValidMask ? FMath::Clamp(SourceResolution, 1, 128) : 1;

	if (!State.ObstacleTexture.IsValid() || State.AllocatedObstacleResolution != DesiredResolution)
	{
		State.ObstacleTexture.SafeRelease();
		const FRDGTextureDesc ObstacleDesc = FRDGTextureDesc::Create3D(
			FIntVector(DesiredResolution, DesiredResolution, DesiredResolution),
			PF_G8,
			FClearValueBinding::Black,
			TexCreate_ShaderResource);
		AllocatePooledTexture(ObstacleDesc, State.ObstacleTexture, TEXT("TimeThiefSmoke.ObstacleMask"));
		State.AllocatedObstacleResolution = DesiredResolution;
		State.UploadedObstacleMaskRevision = MAX_uint32;
		State.TargetObstacleMaskRevision = MAX_uint32;
		State.ObstacleMaskBlendAge = 1.0f;
		State.ObstacleUploadScratch.Reset();
		State.ObstaclePreviousScratch.Reset();
		State.ObstacleTargetScratch.Reset();
	}

	if (!State.ObstacleTexture.IsValid())
	{
		return;
	}

	const int32 DesiredVoxelCount = DesiredResolution * DesiredResolution * DesiredResolution;
	if (State.TargetObstacleMaskRevision != State.Volume.ObstacleMaskRevision)
	{
		State.ObstaclePreviousScratch.SetNumZeroed(DesiredVoxelCount);
		if (State.ObstacleUploadScratch.Num() == DesiredVoxelCount)
		{
			FMemory::Memcpy(
				State.ObstaclePreviousScratch.GetData(),
				State.ObstacleUploadScratch.GetData(),
				DesiredVoxelCount * sizeof(uint8));
		}

		State.ObstacleTargetScratch.SetNumZeroed(DesiredVoxelCount);
		if (bHasValidMask)
		{
			FMemory::Memcpy(
				State.ObstacleTargetScratch.GetData(),
				State.Volume.ObstacleMask.GetData(),
				DesiredVoxelCount * sizeof(uint8));
		}

		State.TargetObstacleMaskRevision = State.Volume.ObstacleMaskRevision;
		State.ObstacleMaskBlendAge = 0.0f;
	}

	if (State.UploadedObstacleMaskRevision == State.TargetObstacleMaskRevision && State.ObstacleMaskBlendAge >= ObstacleMaskBlendDuration)
	{
		return;
	}

	State.ObstacleUploadScratch.SetNumZeroed(DesiredVoxelCount);
	const float BlendAlpha = FMath::Clamp(State.ObstacleMaskBlendAge / ObstacleMaskBlendDuration, 0.0f, 1.0f);
	for (int32 VoxelIndex = 0; VoxelIndex < DesiredVoxelCount; ++VoxelIndex)
	{
		const float PreviousValue = State.ObstaclePreviousScratch.IsValidIndex(VoxelIndex) ? State.ObstaclePreviousScratch[VoxelIndex] : 0.0f;
		const float TargetValue = State.ObstacleTargetScratch.IsValidIndex(VoxelIndex) ? State.ObstacleTargetScratch[VoxelIndex] : 0.0f;
		State.ObstacleUploadScratch[VoxelIndex] = static_cast<uint8>(FMath::RoundToInt(FMath::Lerp(PreviousValue, TargetValue, BlendAlpha)));
	}

	const FUpdateTextureRegion3D UpdateRegion(0, 0, 0, 0, 0, 0, DesiredResolution, DesiredResolution, DesiredResolution);
	GraphBuilder.RHICmdList.UpdateTexture3D(
		State.ObstacleTexture->GetRHI(),
		0,
		UpdateRegion,
		DesiredResolution * sizeof(uint8),
		DesiredResolution * DesiredResolution * sizeof(uint8),
		State.ObstacleUploadScratch.GetData());

	State.ObstacleMaskBlendAge += FMath::Max(LastFrameDeltaSeconds, 1.0f / 120.0f);
	if (State.ObstacleMaskBlendAge >= ObstacleMaskBlendDuration)
	{
		State.UploadedObstacleMaskRevision = State.TargetObstacleMaskRevision;
		if (State.ObstacleTargetScratch.Num() == DesiredVoxelCount)
		{
			FMemory::Memcpy(
				State.ObstacleUploadScratch.GetData(),
				State.ObstacleTargetScratch.GetData(),
				DesiredVoxelCount * sizeof(uint8));
		}
	}
}

void FTimeThiefSmokeViewExtension::EnsureCarrierParticles(FRenderSmokeState& State)
{
	const int32 DesiredCount = FMath::Clamp(State.Volume.Settings.CarrierParticleCount, 1, MaxCarrierParticleCount);
	if (State.AllocatedCarrierParticleCount == DesiredCount && State.CarrierParticles.Num() == DesiredCount)
	{
		return;
	}

	State.AllocatedCarrierParticleCount = DesiredCount;
	State.CarrierParticleBuffers[0].SafeRelease();
	State.CarrierParticleBuffers[1].SafeRelease();
	State.CurrentCarrierParticleIndex = 0;
	State.CarrierParticles.Reset(DesiredCount);
	State.bCarrierParticlesNeedUpload = true;

	FRandomStream RandomStream(State.Volume.SmokeId * 48271 + 1337);
	const float BaseRadius = FMath::Max(1.0f, State.Volume.Settings.CarrierParticleRadius);
	const float DriftSpeed = FMath::Max(0.0f, State.Volume.Settings.CarrierParticleDriftSpeed);
	const float SourceRadius = FMath::Max(8.0f, State.Volume.Settings.PlumeSourceRadius);

	for (int32 ParticleIndex = 0; ParticleIndex < DesiredCount; ++ParticleIndex)
	{
		const FVector Direction = (RandomStream.VRand() + FVector(0.0, 0.0, 0.22)).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
		const FVector SourceJitter = Direction * SourceRadius * RandomStream.FRandRange(0.03f, 0.28f);

		FRenderSmokeState::FCarrierParticle Particle;
		Particle.LocalPosition = FVector3f(SourceJitter);
		Particle.Radius = BaseRadius * RandomStream.FRandRange(0.72f, 1.28f);
		Particle.Phase = RandomStream.FRandRange(0.0f, UE_TWO_PI);
		const FVector InitialVelocity = Direction * DriftSpeed * RandomStream.FRandRange(0.75f, 1.35f);
		Particle.Velocity = FVector3f(InitialVelocity);
		State.CarrierParticles.Add(Particle);
	}
}

void FTimeThiefSmokeViewExtension::EnsureCarrierParticleBuffers(FRDGBuilder& GraphBuilder, FRenderSmokeState& State)
{
	EnsureCarrierParticles(State);

	const int32 DesiredCount = FMath::Clamp(State.Volume.Settings.CarrierParticleCount, 1, MaxCarrierParticleCount);
	FRDGBufferDesc CarrierBufferDesc = FRDGBufferDesc::CreateStructuredDesc(
		sizeof(FTimeThiefSmokeCarrierParticleShaderData),
		DesiredCount);
	CarrierBufferDesc.Usage |=
		EBufferUsageFlags::ShaderResource |
		EBufferUsageFlags::UnorderedAccess |
		EBufferUsageFlags::SourceCopy;

	for (int32 BufferIndex = 0; BufferIndex < 2; ++BufferIndex)
	{
		if (!State.CarrierParticleBuffers[BufferIndex].IsValid())
		{
			AllocatePooledBuffer(CarrierBufferDesc, State.CarrierParticleBuffers[BufferIndex], TEXT("TimeThiefSmoke.CarrierParticles"));
			State.bCarrierParticlesNeedUpload = true;
		}
	}
}

void FTimeThiefSmokeViewExtension::UploadCarrierParticles(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGBufferRef CarrierBuffer)
{
	TArray<FTimeThiefSmokeCarrierParticleShaderData> ShaderParticles;
	ShaderParticles.Reserve(State.CarrierParticles.Num());
	for (const FRenderSmokeState::FCarrierParticle& Particle : State.CarrierParticles)
	{
		FTimeThiefSmokeCarrierParticleShaderData ShaderParticle;
		ShaderParticle.LocalPositionRadius = FVector4f(
			Particle.LocalPosition.X,
			Particle.LocalPosition.Y,
			Particle.LocalPosition.Z,
			Particle.Radius);
		ShaderParticle.VelocityPhase = FVector4f(
			Particle.Velocity.X,
			Particle.Velocity.Y,
			Particle.Velocity.Z,
			Particle.Phase);
		ShaderParticles.Add(ShaderParticle);
	}
	if (ShaderParticles.IsEmpty())
	{
		ShaderParticles.AddDefaulted();
	}

	FRDGBufferRef UploadBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(FTimeThiefSmokeCarrierParticleShaderData), ShaderParticles.Num()),
		TEXT("TimeThiefSmoke.CarrierUpload"));
	GraphBuilder.QueueBufferUpload(UploadBuffer, ShaderParticles.GetData(), ShaderParticles.Num() * sizeof(FTimeThiefSmokeCarrierParticleShaderData));
	AddCopyBufferPass(GraphBuilder, CarrierBuffer, UploadBuffer);
}

void FTimeThiefSmokeViewExtension::AddCarrierParticleUpdatePass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGBufferRef CarrierIn,
	FRDGBufferRef CarrierOut,
	const TArray<FTimeThiefSmokeRendererEvent>& Events,
	float DeltaSeconds)
{
	const int32 CarrierParticleCount = FMath::Clamp(State.Volume.Settings.CarrierParticleCount, 1, MaxCarrierParticleCount);
	TArray<FTimeThiefSmokeEventShaderData> ShaderEvents;
	ShaderEvents.Reserve(Events.Num());
	for (const FTimeThiefSmokeRendererEvent& Event : Events)
	{
		ShaderEvents.Add(ToShaderEvent(Event));
	}
	if (ShaderEvents.IsEmpty())
	{
		ShaderEvents.AddDefaulted();
	}

	FRDGBufferRef EventBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(FTimeThiefSmokeEventShaderData), ShaderEvents.Num()),
		TEXT("TimeThiefSmoke.CarrierEvents"));
	GraphBuilder.QueueBufferUpload(EventBuffer, ShaderEvents.GetData(), ShaderEvents.Num() * sizeof(FTimeThiefSmokeEventShaderData));

	TShaderMapRef<FTimeThiefSmokeCarrierUpdateCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokeCarrierUpdateCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeCarrierUpdateCS::FParameters>();
	PassParameters->CarrierParticlesIn = GraphBuilder.CreateSRV(CarrierIn);
	PassParameters->Events = GraphBuilder.CreateSRV(EventBuffer);
	PassParameters->OutCarrierParticles = GraphBuilder.CreateUAV(CarrierOut);
	PassParameters->DeltaSeconds = DeltaSeconds;
	PassParameters->DriftSpeed = State.Volume.Settings.CarrierParticleDriftSpeed;
	PassParameters->InteractionStrength = State.Volume.Settings.CarrierParticleInteractionStrength;
	PassParameters->CarrierParticleCount = CarrierParticleCount;
	PassParameters->EventCount = Events.Num();
	PassParameters->BoundsExtent = FVector3f(State.Volume.BoundsExtent);
	PassParameters->LocalToWorld = State.Volume.LocalToWorld.ToMatrixWithScale();
	PassParameters->WorldToLocal = State.Volume.LocalToWorld.ToInverseMatrixWithScale();

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.CarrierUpdate SmokeId=%d Events=%d", State.Volume.SmokeId, Events.Num()),
		ComputeShader,
		PassParameters,
		FIntVector(FMath::DivideAndRoundUp(CarrierParticleCount, 64), 1, 1));
}

void FTimeThiefSmokeViewExtension::SimulateSmoke(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	float DeltaSeconds)
{
	EnsureResources(GraphBuilder, State);
	EnsureCarrierParticleBuffers(GraphBuilder, State);

	FRDGTextureRef DensityTextures[2] =
	{
		GraphBuilder.RegisterExternalTexture(State.DensityTextures[0]),
		GraphBuilder.RegisterExternalTexture(State.DensityTextures[1])
	};
	FRDGTextureRef VelocityTextures[2] =
	{
		GraphBuilder.RegisterExternalTexture(State.VelocityTextures[0]),
		GraphBuilder.RegisterExternalTexture(State.VelocityTextures[1])
	};
	FRDGTextureRef PressureTextures[2] =
	{
		GraphBuilder.RegisterExternalTexture(State.PressureTextures[0]),
		GraphBuilder.RegisterExternalTexture(State.PressureTextures[1])
	};
	FRDGTextureRef BulletSuppressionTextures[2] =
	{
		GraphBuilder.RegisterExternalTexture(State.BulletSuppressionTextures[0]),
		GraphBuilder.RegisterExternalTexture(State.BulletSuppressionTextures[1])
	};
	FRDGTextureRef DivergenceTexture = GraphBuilder.RegisterExternalTexture(State.DivergenceTexture);
	FRDGBufferRef CarrierParticleBuffers[2] =
	{
		GraphBuilder.RegisterExternalBuffer(State.CarrierParticleBuffers[0], TEXT("TimeThiefSmoke.CarrierParticles0")),
		GraphBuilder.RegisterExternalBuffer(State.CarrierParticleBuffers[1], TEXT("TimeThiefSmoke.CarrierParticles1"))
	};

	if (State.bCarrierParticlesNeedUpload)
	{
		UploadCarrierParticles(GraphBuilder, State, CarrierParticleBuffers[0]);
		UploadCarrierParticles(GraphBuilder, State, CarrierParticleBuffers[1]);
		State.CurrentCarrierParticleIndex = 0;
		State.bCarrierParticlesNeedUpload = false;
	}

	const int32 CarrierReadIndex = State.CurrentCarrierParticleIndex;
	const int32 CarrierWriteIndex = 1 - State.CurrentCarrierParticleIndex;
	AddCarrierParticleUpdatePass(GraphBuilder, State, CarrierParticleBuffers[CarrierReadIndex], CarrierParticleBuffers[CarrierWriteIndex], State.PendingEvents, DeltaSeconds);
	State.CurrentCarrierParticleIndex = CarrierWriteIndex;

	if (State.bNeedsInit)
	{
		AddInitPass(GraphBuilder, State, DensityTextures[State.CurrentDensityIndex], VelocityTextures[State.CurrentVelocityIndex]);
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(BulletSuppressionTextures[0]), 0.0f);
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(BulletSuppressionTextures[1]), 0.0f);
		State.bNeedsInit = false;
	}

	const int32 SuppressionReadIndex = State.CurrentBulletSuppressionIndex;
	const int32 SuppressionWriteIndex = 1 - State.CurrentBulletSuppressionIndex;
	AddBulletSuppressionPass(GraphBuilder, State, BulletSuppressionTextures[SuppressionReadIndex], BulletSuppressionTextures[SuppressionWriteIndex], State.PendingEvents, DeltaSeconds);
	State.CurrentBulletSuppressionIndex = SuppressionWriteIndex;

	const int32 ReadDensityIndex = State.CurrentDensityIndex;
	const int32 ReadVelocityIndex = State.CurrentVelocityIndex;
	const int32 WriteDensityIndex = 1 - State.CurrentDensityIndex;
	const int32 WriteVelocityIndex = 1 - State.CurrentVelocityIndex;
	AddSimulatePass(GraphBuilder, State, DensityTextures[ReadDensityIndex], VelocityTextures[ReadVelocityIndex], BulletSuppressionTextures[State.CurrentBulletSuppressionIndex], CarrierParticleBuffers[State.CurrentCarrierParticleIndex], DensityTextures[WriteDensityIndex], VelocityTextures[WriteVelocityIndex], DeltaSeconds);
	State.CurrentDensityIndex = WriteDensityIndex;
	State.CurrentVelocityIndex = WriteVelocityIndex;

	if (!State.PendingEvents.IsEmpty())
	{
		const int32 EventReadDensityIndex = State.CurrentDensityIndex;
		const int32 EventReadVelocityIndex = State.CurrentVelocityIndex;
		const int32 EventWriteDensityIndex = 1 - State.CurrentDensityIndex;
		const int32 EventWriteVelocityIndex = 1 - State.CurrentVelocityIndex;
		AddApplyEventsPass(GraphBuilder, State, DensityTextures[EventReadDensityIndex], VelocityTextures[EventReadVelocityIndex], DensityTextures[EventWriteDensityIndex], VelocityTextures[EventWriteVelocityIndex], State.PendingEvents, DeltaSeconds);
		State.CurrentDensityIndex = EventWriteDensityIndex;
		State.CurrentVelocityIndex = EventWriteVelocityIndex;
		if (HasEventType(State.PendingEvents, ETimeThiefSmokeRendererInteractionType::ActorPush))
		{
			const int32 ObstacleReadDensityIndex = State.CurrentDensityIndex;
			const int32 ObstacleReadVelocityIndex = State.CurrentVelocityIndex;
			const int32 ObstacleWriteDensityIndex = 1 - State.CurrentDensityIndex;
			const int32 ObstacleWriteVelocityIndex = 1 - State.CurrentVelocityIndex;
			AddDynamicObstaclePass(GraphBuilder, State, DensityTextures[ObstacleReadDensityIndex], VelocityTextures[ObstacleReadVelocityIndex], DensityTextures[ObstacleWriteDensityIndex], VelocityTextures[ObstacleWriteVelocityIndex], State.PendingEvents);
			State.CurrentDensityIndex = ObstacleWriteDensityIndex;
			State.CurrentVelocityIndex = ObstacleWriteVelocityIndex;
		}
		State.PendingEvents.Reset();
	}

	AddDivergencePass(GraphBuilder, State, VelocityTextures[State.CurrentVelocityIndex], DivergenceTexture, PressureTextures[0]);
	int32 CurrentPressureIndex = 0;
	const int32 PressureIterations = FMath::Clamp(State.Volume.Settings.PressureIterations, 1, 64);
	for (int32 Iteration = 0; Iteration < PressureIterations; ++Iteration)
	{
		const int32 NextPressureIndex = 1 - CurrentPressureIndex;
		AddPressureJacobiPass(GraphBuilder, State, PressureTextures[CurrentPressureIndex], DivergenceTexture, PressureTextures[NextPressureIndex]);
		CurrentPressureIndex = NextPressureIndex;
	}

	const int32 ProjectedVelocityIndex = 1 - State.CurrentVelocityIndex;
	AddProjectVelocityPass(GraphBuilder, State, VelocityTextures[State.CurrentVelocityIndex], PressureTextures[CurrentPressureIndex], VelocityTextures[ProjectedVelocityIndex]);
	State.CurrentVelocityIndex = ProjectedVelocityIndex;
}

void FTimeThiefSmokeViewExtension::AddInitPass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGTextureRef DensityTexture,
	FRDGTextureRef VelocityTexture)
{
	const FIntVector GridSize = MakeGridSize(State.AllocatedResolution);
	const FIntVector GroupCount = MakeGroupCount(GridSize);

	TShaderMapRef<FTimeThiefSmokeInitCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokeInitCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeInitCS::FParameters>();
	PassParameters->OutDensity = GraphBuilder.CreateUAV(DensityTexture);
	PassParameters->OutVelocity = GraphBuilder.CreateUAV(VelocityTexture);
	PassParameters->GridResolution = GridSize;
	PassParameters->BoundsExtent = FVector3f(State.Volume.BoundsExtent);
	PassParameters->InitialDensity = State.Volume.Settings.InitialDensity;

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.Init SmokeId=%d", State.Volume.SmokeId),
		ComputeShader,
		PassParameters,
		GroupCount);
}

void FTimeThiefSmokeViewExtension::AddApplyEventsPass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGTextureRef DensityIn,
	FRDGTextureRef VelocityIn,
	FRDGTextureRef DensityOut,
	FRDGTextureRef VelocityOut,
	const TArray<FTimeThiefSmokeRendererEvent>& Events,
	float DeltaSeconds)
{
	const FIntVector GridSize = MakeGridSize(State.AllocatedResolution);
	const FIntVector GroupCount = MakeGroupCount(GridSize);

	TArray<FTimeThiefSmokeEventShaderData> ShaderEvents;
	ShaderEvents.Reserve(Events.Num());
	for (const FTimeThiefSmokeRendererEvent& Event : Events)
	{
		ShaderEvents.Add(ToShaderEvent(Event));
	}
	if (ShaderEvents.IsEmpty())
	{
		ShaderEvents.AddDefaulted();
	}

	FRDGBufferRef EventBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(FTimeThiefSmokeEventShaderData), ShaderEvents.Num()),
		TEXT("TimeThiefSmoke.Events"));
	GraphBuilder.QueueBufferUpload(EventBuffer, ShaderEvents.GetData(), ShaderEvents.Num() * sizeof(FTimeThiefSmokeEventShaderData));

	TShaderMapRef<FTimeThiefSmokeApplyEventsCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokeApplyEventsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeApplyEventsCS::FParameters>();
	PassParameters->DensityIn = DensityIn;
	PassParameters->VelocityIn = VelocityIn;
	PassParameters->ObstacleTexture = GraphBuilder.RegisterExternalTexture(State.ObstacleTexture);
	PassParameters->VolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->Events = GraphBuilder.CreateSRV(EventBuffer);
	PassParameters->OutDensity = GraphBuilder.CreateUAV(DensityOut);
	PassParameters->OutVelocity = GraphBuilder.CreateUAV(VelocityOut);
	PassParameters->GridResolution = GridSize;
	PassParameters->BoundsExtent = FVector3f(State.Volume.BoundsExtent);
	PassParameters->DeltaSeconds = DeltaSeconds;
	PassParameters->EventCount = Events.Num();
	PassParameters->LocalToWorld = State.Volume.LocalToWorld.ToMatrixWithScale();

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.ApplyEvents SmokeId=%d Events=%d", State.Volume.SmokeId, Events.Num()),
		ComputeShader,
		PassParameters,
		GroupCount);
}

void FTimeThiefSmokeViewExtension::AddBulletSuppressionPass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGTextureRef SuppressionIn,
	FRDGTextureRef SuppressionOut,
	const TArray<FTimeThiefSmokeRendererEvent>& Events,
	float DeltaSeconds)
{
	const FIntVector GridSize = MakeGridSize(State.AllocatedResolution);
	const FIntVector GroupCount = MakeGroupCount(GridSize);

	TArray<FTimeThiefSmokeEventShaderData> ShaderEvents;
	ShaderEvents.Reserve(Events.Num());
	for (const FTimeThiefSmokeRendererEvent& Event : Events)
	{
		ShaderEvents.Add(ToShaderEvent(Event));
	}
	if (ShaderEvents.IsEmpty())
	{
		ShaderEvents.AddDefaulted();
	}

	FRDGBufferRef EventBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(FTimeThiefSmokeEventShaderData), ShaderEvents.Num()),
		TEXT("TimeThiefSmoke.BulletSuppressionEvents"));
	GraphBuilder.QueueBufferUpload(EventBuffer, ShaderEvents.GetData(), ShaderEvents.Num() * sizeof(FTimeThiefSmokeEventShaderData));

	TShaderMapRef<FTimeThiefSmokeBulletSuppressCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokeBulletSuppressCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeBulletSuppressCS::FParameters>();
	PassParameters->SuppressionIn = SuppressionIn;
	PassParameters->VolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->Events = GraphBuilder.CreateSRV(EventBuffer);
	PassParameters->OutSuppression = GraphBuilder.CreateUAV(SuppressionOut);
	PassParameters->GridResolution = GridSize;
	PassParameters->BoundsExtent = FVector3f(State.Volume.BoundsExtent);
	PassParameters->DeltaSeconds = DeltaSeconds;
	PassParameters->SuppressionLife = FMath::Max(0.05f, State.Volume.Settings.BulletWakeMaxVisibleLife);
	PassParameters->EventCount = Events.Num();
	PassParameters->LocalToWorld = State.Volume.LocalToWorld.ToMatrixWithScale();

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.BulletSuppress SmokeId=%d Events=%d", State.Volume.SmokeId, Events.Num()),
		ComputeShader,
		PassParameters,
		GroupCount);
}

void FTimeThiefSmokeViewExtension::AddDynamicObstaclePass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGTextureRef DensityIn,
	FRDGTextureRef VelocityIn,
	FRDGTextureRef DensityOut,
	FRDGTextureRef VelocityOut,
	const TArray<FTimeThiefSmokeRendererEvent>& Events)
{
	const FIntVector GridSize = MakeGridSize(State.AllocatedResolution);
	const FIntVector GroupCount = MakeGroupCount(GridSize);

	TArray<FTimeThiefSmokeEventShaderData> ShaderEvents;
	ShaderEvents.Reserve(Events.Num());
	for (const FTimeThiefSmokeRendererEvent& Event : Events)
	{
		ShaderEvents.Add(ToShaderEvent(Event));
	}
	if (ShaderEvents.IsEmpty())
	{
		ShaderEvents.AddDefaulted();
	}

	FRDGBufferRef EventBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(FTimeThiefSmokeEventShaderData), ShaderEvents.Num()),
		TEXT("TimeThiefSmoke.DynamicObstacleEvents"));
	GraphBuilder.QueueBufferUpload(EventBuffer, ShaderEvents.GetData(), ShaderEvents.Num() * sizeof(FTimeThiefSmokeEventShaderData));

	TShaderMapRef<FTimeThiefSmokeDynamicObstacleCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokeDynamicObstacleCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeDynamicObstacleCS::FParameters>();
	PassParameters->DensityIn = DensityIn;
	PassParameters->VelocityIn = VelocityIn;
	PassParameters->ObstacleTexture = GraphBuilder.RegisterExternalTexture(State.ObstacleTexture);
	PassParameters->VolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->Events = GraphBuilder.CreateSRV(EventBuffer);
	PassParameters->OutDensity = GraphBuilder.CreateUAV(DensityOut);
	PassParameters->OutVelocity = GraphBuilder.CreateUAV(VelocityOut);
	PassParameters->GridResolution = GridSize;
	PassParameters->BoundsExtent = FVector3f(State.Volume.BoundsExtent);
	PassParameters->EventCount = Events.Num();
	PassParameters->LocalToWorld = State.Volume.LocalToWorld.ToMatrixWithScale();

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.DynamicObstacle SmokeId=%d Events=%d", State.Volume.SmokeId, Events.Num()),
		ComputeShader,
		PassParameters,
		GroupCount);
}

void FTimeThiefSmokeViewExtension::AddSimulatePass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGTextureRef DensityIn,
	FRDGTextureRef VelocityIn,
	FRDGTextureRef BulletSuppressionTexture,
	FRDGBufferRef CarrierBuffer,
	FRDGTextureRef DensityOut,
	FRDGTextureRef VelocityOut,
	float DeltaSeconds)
{
	const FIntVector GridSize = MakeGridSize(State.AllocatedResolution);
	const FIntVector GroupCount = MakeGroupCount(GridSize);

	TShaderMapRef<FTimeThiefSmokeSimulateCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokeSimulateCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeSimulateCS::FParameters>();
	PassParameters->DensityIn = DensityIn;
	PassParameters->VelocityIn = VelocityIn;
	PassParameters->ObstacleTexture = GraphBuilder.RegisterExternalTexture(State.ObstacleTexture);
	PassParameters->BulletSuppressionTexture = BulletSuppressionTexture;
	PassParameters->VolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->CarrierParticles = GraphBuilder.CreateSRV(CarrierBuffer);
	PassParameters->OutDensity = GraphBuilder.CreateUAV(DensityOut);
	PassParameters->OutVelocity = GraphBuilder.CreateUAV(VelocityOut);
	PassParameters->GridResolution = GridSize;
	PassParameters->BoundsExtent = FVector3f(State.Volume.BoundsExtent);
	PassParameters->DeltaSeconds = DeltaSeconds;
	PassParameters->InitialDensity = State.Volume.Settings.InitialDensity;
	PassParameters->AgeSeconds = State.Volume.AgeSeconds;
	PassParameters->DurationSeconds = State.Volume.DurationSeconds;
	PassParameters->SmokeFadeOutDuration = State.Volume.Settings.SmokeFadeOutDuration;
	PassParameters->PlumeEmissionDuration = State.Volume.Settings.PlumeEmissionDuration;
	PassParameters->PlumeSourceRadius = State.Volume.Settings.PlumeSourceRadius;
	PassParameters->PlumeExpansionVelocity = State.Volume.Settings.PlumeExpansionVelocity;
	PassParameters->PlumeRiseVelocity = State.Volume.Settings.PlumeRiseVelocity;
	PassParameters->DensityDissipation = State.Volume.Settings.DensityDissipation;
	PassParameters->VelocityDamping = State.Volume.Settings.VelocityDamping;
	PassParameters->VorticityStrength = State.Volume.Settings.VorticityStrength;
	PassParameters->bUseMacCormackAdvection = State.Volume.Settings.bUseMacCormackAdvection ? 1u : 0u;
	PassParameters->CarrierParticleCount = FMath::Min(State.AllocatedCarrierParticleCount, MaxCarrierParticleCount);
	PassParameters->LocalToWorld = State.Volume.LocalToWorld.ToMatrixWithScale();
	PassParameters->WorldToLocal = State.Volume.LocalToWorld.ToInverseMatrixWithScale();

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.Simulate SmokeId=%d", State.Volume.SmokeId),
		ComputeShader,
		PassParameters,
		GroupCount);
}

void FTimeThiefSmokeViewExtension::AddDivergencePass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGTextureRef VelocityIn,
	FRDGTextureRef DivergenceOut,
	FRDGTextureRef PressureOut)
{
	const FIntVector GridSize = MakeGridSize(State.AllocatedResolution);
	const FIntVector GroupCount = MakeGroupCount(GridSize);
	const FVector3f GridSizeFloat(static_cast<float>(GridSize.X), static_cast<float>(GridSize.Y), static_cast<float>(GridSize.Z));
	const FVector3f CellSize = (FVector3f(State.Volume.BoundsExtent) * 2.0f) / GridSizeFloat;

	TShaderMapRef<FTimeThiefSmokeDivergenceCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokeDivergenceCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeDivergenceCS::FParameters>();
	PassParameters->GridResolution = GridSize;
	PassParameters->CellSize = CellSize;
	PassParameters->VelocityIn = VelocityIn;
	PassParameters->ObstacleTexture = GraphBuilder.RegisterExternalTexture(State.ObstacleTexture);
	PassParameters->VolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->OutDivergence = GraphBuilder.CreateUAV(DivergenceOut);
	PassParameters->OutPressure = GraphBuilder.CreateUAV(PressureOut);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.Divergence SmokeId=%d", State.Volume.SmokeId),
		ComputeShader,
		PassParameters,
		GroupCount);
}

void FTimeThiefSmokeViewExtension::AddPressureJacobiPass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGTextureRef PressureIn,
	FRDGTextureRef DivergenceIn,
	FRDGTextureRef PressureOut)
{
	const FIntVector GridSize = MakeGridSize(State.AllocatedResolution);
	const FIntVector GroupCount = MakeGroupCount(GridSize);

	TShaderMapRef<FTimeThiefSmokePressureJacobiCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokePressureJacobiCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokePressureJacobiCS::FParameters>();
	PassParameters->GridResolution = GridSize;
	PassParameters->PressureIn = PressureIn;
	PassParameters->DivergenceIn = DivergenceIn;
	PassParameters->OutPressure = GraphBuilder.CreateUAV(PressureOut);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.PressureJacobi SmokeId=%d", State.Volume.SmokeId),
		ComputeShader,
		PassParameters,
		GroupCount);
}

void FTimeThiefSmokeViewExtension::AddProjectVelocityPass(
	FRDGBuilder& GraphBuilder,
	FRenderSmokeState& State,
	FRDGTextureRef VelocityIn,
	FRDGTextureRef PressureIn,
	FRDGTextureRef VelocityOut)
{
	const FIntVector GridSize = MakeGridSize(State.AllocatedResolution);
	const FIntVector GroupCount = MakeGroupCount(GridSize);
	const FVector3f GridSizeFloat(static_cast<float>(GridSize.X), static_cast<float>(GridSize.Y), static_cast<float>(GridSize.Z));
	const FVector3f CellSize = (FVector3f(State.Volume.BoundsExtent) * 2.0f) / GridSizeFloat;

	TShaderMapRef<FTimeThiefSmokeProjectVelocityCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FTimeThiefSmokeProjectVelocityCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeProjectVelocityCS::FParameters>();
	PassParameters->GridResolution = GridSize;
	PassParameters->CellSize = CellSize;
	PassParameters->VelocityIn = VelocityIn;
	PassParameters->PressureIn = PressureIn;
	PassParameters->ObstacleTexture = GraphBuilder.RegisterExternalTexture(State.ObstacleTexture);
	PassParameters->VolumeSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->OutVelocity = GraphBuilder.CreateUAV(VelocityOut);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TimeThiefSmoke.ProjectVelocity SmokeId=%d", State.Volume.SmokeId),
		ComputeShader,
		PassParameters,
		GroupCount);
}
