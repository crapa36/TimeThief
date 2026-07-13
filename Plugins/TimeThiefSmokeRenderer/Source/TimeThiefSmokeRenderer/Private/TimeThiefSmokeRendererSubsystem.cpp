#include "TimeThiefSmokeRendererSubsystem.h"

#include "RenderingThread.h"
#include "TimeThiefSmokeTestBridge.h"
#include "TimeThiefSmokeViewExtension.h"

void UTimeThiefSmokeRendererSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ViewExtension = FSceneViewExtensions::NewExtension<FTimeThiefSmokeViewExtension>();

	TSharedPtr<FTimeThiefSmokeViewExtension, ESPMode::ThreadSafe> Extension = ViewExtension;
	if (Extension.IsValid())
	{
		ENQUEUE_RENDER_COMMAND(TimeThiefSmokeWarmupCommand)(
			[Extension](FRHICommandListImmediate& RHICmdList)
			{
				Extension->PreAllocateWarmupTextures_RenderThread(RHICmdList);
			});
	}
}

void UTimeThiefSmokeRendererSubsystem::Deinitialize()
{
	TSharedPtr<FTimeThiefSmokeViewExtension, ESPMode::ThreadSafe> Extension = ViewExtension;
	if (Extension.IsValid())
	{
		ENQUEUE_RENDER_COMMAND(TimeThiefSmokeClearRenderer)(
			[Extension](FRHICommandListImmediate& RHICmdList)
			{
				Extension->Clear_RenderThread();
			});
	}

	ViewExtension.Reset();
	Super::Deinitialize();
}

void UTimeThiefSmokeRendererSubsystem::SubmitFrame(FTimeThiefSmokeRendererFrame Frame)
{
	TSharedPtr<FTimeThiefSmokeViewExtension, ESPMode::ThreadSafe> Extension = ViewExtension;
	if (!Extension.IsValid())
	{
		return;
	}

	ENQUEUE_RENDER_COMMAND(TimeThiefSmokeSubmitFrame)(
		[Extension, RenderFrame = MoveTemp(Frame)](FRHICommandListImmediate& RHICmdList) mutable
		{
			if (FTimeThiefSmokeTestBridge::IsActive())
			{
				FTimeThiefSmokeTestEvent Event;
				Event.Type = TEXT("renderer_frame_received");
				Event.Count = RenderFrame.Events.Num();
				Event.FrameId = GFrameCounter;
				for (const FTimeThiefSmokeRendererVolume& Volume : RenderFrame.Volumes)
				{
					Event.SmokeIds.Add(Volume.SmokeId);
				}
				FTimeThiefSmokeTestBridge::Emit(Event);
			}
			Extension->SubmitFrame_RenderThread(MoveTemp(RenderFrame));
		});
}
