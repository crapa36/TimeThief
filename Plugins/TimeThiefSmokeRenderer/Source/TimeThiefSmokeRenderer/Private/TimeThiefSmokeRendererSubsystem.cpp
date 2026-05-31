#include "TimeThiefSmokeRendererSubsystem.h"

#include "RenderingThread.h"
#include "TimeThiefSmokeViewExtension.h"

void UTimeThiefSmokeRendererSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ViewExtension = FSceneViewExtensions::NewExtension<FTimeThiefSmokeViewExtension>();
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
			Extension->SubmitFrame_RenderThread(MoveTemp(RenderFrame));
		});
}
