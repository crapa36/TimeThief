#pragma once

#include "CoreMinimal.h"
#include "DynamicRHI.h"
#include "RenderGraphUtils.h"
#include "RHIGPUReadback.h"
#include "TimeThiefSmokeTestBridge.h"

BEGIN_SHADER_PARAMETER_STRUCT(FTimeThiefSmokeTimestampPassParameters, )
	RDG_TEXTURE_ACCESS(OrderingTexture, ERHIAccess::SRVCompute)
END_SHADER_PARAMETER_STRUCT()

class FTimeThiefSmokeTestGpuProfiler
{
public:
	struct FPendingQuery
	{
		FRenderQueryRHIRef StartQuery;
		FRenderQueryRHIRef EndQuery;
		FTimeThiefSmokeTestGpuPassResult Metadata;
		TUniquePtr<FRHIGPUBufferReadback> StepStatsReadback;
		uint64 QueuedFrame = 0;
	};

	using FQueryHandle = TSharedPtr<FPendingQuery, ESPMode::ThreadSafe>;
	FQueryHandle BeginRasterPass(FTimeThiefSmokeTestGpuPassResult Metadata);
	void WriteRasterStart(FRHICommandList& RHICmdList, const FQueryHandle& Query) const;
	void EndRasterPass(FRDGBuilder& GraphBuilder, FRDGTextureRef OrderingTexture, const FQueryHandle& Query, FRDGBufferRef StepStatsBuffer = nullptr);
	void PollResults_RenderThread();
	void Reset_RenderThread();

	template <typename TShaderClass>
	void AddPass(
		FRDGBuilder& GraphBuilder,
		FRDGEventName&& EventName,
		const TShaderRef<TShaderClass>& ComputeShader,
		typename TShaderClass::FParameters* Parameters,
		FIntVector GroupCount,
		FTimeThiefSmokeTestGpuPassResult Metadata)
	{
		Metadata.bPassExecuted = true;
		Metadata.DispatchGroupCount = FMath::Clamp<int64>(
			static_cast<int64>(GroupCount.X) * GroupCount.Y * GroupCount.Z,
			0,
			MAX_int32);
		if (!ShouldMeasure())
		{
			FComputeShaderUtils::AddPass(GraphBuilder, MoveTemp(EventName), ComputeShader, Parameters, GroupCount);
			return;
		}

		FComputeShaderUtils::ValidateGroupCount(GroupCount);
		ClearUnusedGraphResources(ComputeShader, Parameters);
		TSharedPtr<FPendingQuery, ESPMode::ThreadSafe> Query = CreateQuery(MoveTemp(Metadata));
		GraphBuilder.AddPass(
			MoveTemp(EventName),
			Parameters,
			ERDGPassFlags::Compute,
			[Parameters, ComputeShader, GroupCount, Query](FRDGAsyncTask, FRHIComputeCommandList& RHICmdList)
			{
				GDynamicRHI->RHIEndRenderQuery_TopOfPipe(RHICmdList, Query->StartQuery);
				FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *Parameters, GroupCount);
				GDynamicRHI->RHIEndRenderQuery_TopOfPipe(RHICmdList, Query->EndQuery);
			});
	}

	template <typename TShaderClass>
	void AddPass(
		FRDGBuilder& GraphBuilder,
		FRDGEventName&& EventName,
		const TShaderRef<TShaderClass>& ComputeShader,
		typename TShaderClass::FParameters* Parameters,
		FRDGBufferRef IndirectArgsBuffer,
		uint32 IndirectArgsOffset,
		FTimeThiefSmokeTestGpuPassResult Metadata)
	{
		Metadata.bPassExecuted = true;
		if (!ShouldMeasure())
		{
			FComputeShaderUtils::AddPass(GraphBuilder, MoveTemp(EventName), ComputeShader, Parameters, IndirectArgsBuffer, IndirectArgsOffset);
			return;
		}

		FComputeShaderUtils::ValidateIndirectArgsBuffer(IndirectArgsBuffer, IndirectArgsOffset);
		ClearUnusedGraphResources(ComputeShader, Parameters, { IndirectArgsBuffer });
		TSharedPtr<FPendingQuery, ESPMode::ThreadSafe> Query = CreateQuery(MoveTemp(Metadata));
		GraphBuilder.AddPass(
			MoveTemp(EventName),
			Parameters,
			ERDGPassFlags::Compute,
			[Parameters, ComputeShader, IndirectArgsBuffer, IndirectArgsOffset, Query](FRDGAsyncTask, FRHIComputeCommandList& RHICmdList)
			{
				IndirectArgsBuffer->MarkResourceAsUsed();
				GDynamicRHI->RHIEndRenderQuery_TopOfPipe(RHICmdList, Query->StartQuery);
				FComputeShaderUtils::DispatchIndirect(RHICmdList, ComputeShader, *Parameters, IndirectArgsBuffer->GetIndirectRHICallBuffer(), IndirectArgsOffset);
				GDynamicRHI->RHIEndRenderQuery_TopOfPipe(RHICmdList, Query->EndQuery);
			});
	}

private:
	bool ShouldMeasure() const;
	TSharedPtr<FPendingQuery, ESPMode::ThreadSafe> CreateQuery(FTimeThiefSmokeTestGpuPassResult&& Metadata);
	TArray<TSharedPtr<FPendingQuery, ESPMode::ThreadSafe>> PendingQueries;
};
