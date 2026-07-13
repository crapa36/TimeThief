#include "TimeThiefSmokeTestGpuProfiler.h"

#include "DynamicRHI.h"
#include "RHIGlobals.h"

bool FTimeThiefSmokeTestGpuProfiler::ShouldMeasure() const
{
	return FTimeThiefSmokeTestBridge::IsMeasurementActive() && GSupportsTimestampRenderQueries;
}

FTimeThiefSmokeTestGpuProfiler::FQueryHandle FTimeThiefSmokeTestGpuProfiler::BeginRasterPass(FTimeThiefSmokeTestGpuPassResult Metadata)
{
	return ShouldMeasure() ? CreateQuery(MoveTemp(Metadata)) : nullptr;
}

void FTimeThiefSmokeTestGpuProfiler::WriteRasterStart(FRHICommandList& RHICmdList, const FQueryHandle& Query) const
{
	if (Query.IsValid())
	{
		GDynamicRHI->RHIEndRenderQuery_TopOfPipe(RHICmdList, Query->StartQuery);
	}
}

void FTimeThiefSmokeTestGpuProfiler::EndRasterPass(
	FRDGBuilder& GraphBuilder,
	FRDGTextureRef OrderingTexture,
	const FQueryHandle& Query,
	FRDGBufferRef StepStatsBuffer)
{
	if (!Query.IsValid() || !OrderingTexture)
	{
		return;
	}
	FTimeThiefSmokeTimestampPassParameters* PassParameters = GraphBuilder.AllocParameters<FTimeThiefSmokeTimestampPassParameters>();
	PassParameters->OrderingTexture = OrderingTexture;
	GraphBuilder.AddPass(
		RDG_EVENT_NAME("TimeThiefSmoke.TestTimestampEnd"),
		PassParameters,
		ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
		[Query](FRHIComputeCommandList& RHICmdList)
		{
			GDynamicRHI->RHIEndRenderQuery_TopOfPipe(RHICmdList, Query->EndQuery);
		});
	if (StepStatsBuffer)
	{
		Query->StepStatsReadback = MakeUnique<FRHIGPUBufferReadback>(TEXT("TimeThiefSmoke.CompositeStepStatsReadback"));
		AddEnqueueCopyPass(GraphBuilder, Query->StepStatsReadback.Get(), StepStatsBuffer, 8u * sizeof(uint32));
	}
}

TSharedPtr<FTimeThiefSmokeTestGpuProfiler::FPendingQuery, ESPMode::ThreadSafe> FTimeThiefSmokeTestGpuProfiler::CreateQuery(FTimeThiefSmokeTestGpuPassResult&& Metadata)
{
	check(IsInRenderingThread());
	TSharedPtr<FPendingQuery, ESPMode::ThreadSafe> Query = MakeShared<FPendingQuery, ESPMode::ThreadSafe>();
	Query->StartQuery = RHICreateRenderQuery(RQT_AbsoluteTime);
	Query->EndQuery = RHICreateRenderQuery(RQT_AbsoluteTime);
	Query->Metadata = MoveTemp(Metadata);
	Query->Metadata.FrameId = GFrameCounter;
	Query->Metadata.Phase = FTimeThiefSmokeTestBridge::GetPhase();
	Query->QueuedFrame = GFrameCounter;
	PendingQueries.Add(Query);
	FTimeThiefSmokeTestBridge::NotifyGpuQueryQueued();
	return Query;
}

void FTimeThiefSmokeTestGpuProfiler::PollResults_RenderThread()
{
	check(IsInRenderingThread());
	for (int32 Index = PendingQueries.Num() - 1; Index >= 0; --Index)
	{
		const TSharedPtr<FPendingQuery, ESPMode::ThreadSafe>& Query = PendingQueries[Index];
		uint64 StartMicroseconds = 0;
		uint64 EndMicroseconds = 0;
		const bool bStepStatsReady = !Query.IsValid() || !Query->StepStatsReadback || Query->StepStatsReadback->IsReady();
		if (Query.IsValid() && bStepStatsReady &&
			RHIGetRenderQueryResult(Query->StartQuery, StartMicroseconds, false) &&
			RHIGetRenderQueryResult(Query->EndQuery, EndMicroseconds, false))
		{
			Query->Metadata.DurationMilliseconds = EndMicroseconds >= StartMicroseconds
				? static_cast<double>(EndMicroseconds - StartMicroseconds) / 1000.0
				: 0.0;
			if (Query->StepStatsReadback)
			{
				const uint32* Stats = static_cast<const uint32*>(Query->StepStatsReadback->Lock(8u * sizeof(uint32)));
				if (Stats)
				{
					const uint32 ResolvedCount = Stats[3];
					const uint32 ExecutedCount = Stats[7];
					if (ResolvedCount > 0u)
					{
						Query->Metadata.ActualResolvedStepMin = FMath::Max(0, 256 - static_cast<int32>(Stats[0]));
						Query->Metadata.ActualResolvedStepMax = static_cast<int32>(Stats[1]);
						Query->Metadata.ActualResolvedStepAverage = static_cast<float>(Stats[2]) / static_cast<float>(ResolvedCount);
					}
					if (ExecutedCount > 0u)
					{
						Query->Metadata.ActualExecutedStepMin = FMath::Max(0, 256 - static_cast<int32>(Stats[4]));
						Query->Metadata.ActualExecutedStepMax = static_cast<int32>(Stats[5]);
						Query->Metadata.ActualExecutedStepAverage = static_cast<float>(Stats[6]) / static_cast<float>(ExecutedCount);
					}
				}
				Query->StepStatsReadback->Unlock();
			}
			FTimeThiefSmokeTestBridge::EmitGpuPass(Query->Metadata);
			FTimeThiefSmokeTestBridge::NotifyGpuQueryFinished();
			PendingQueries.RemoveAtSwap(Index);
		}
		else if (GFrameCounter - Query->QueuedFrame > 240)
		{
			FTimeThiefSmokeTestBridge::NotifyGpuQueryFinished();
			PendingQueries.RemoveAtSwap(Index);
		}
	}
}

void FTimeThiefSmokeTestGpuProfiler::Reset_RenderThread()
{
	check(IsInRenderingThread());
	for (int32 Index = 0; Index < PendingQueries.Num(); ++Index) FTimeThiefSmokeTestBridge::NotifyGpuQueryFinished();
	PendingQueries.Reset();
}
