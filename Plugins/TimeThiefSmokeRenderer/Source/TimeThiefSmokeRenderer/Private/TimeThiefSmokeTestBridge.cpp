#include "TimeThiefSmokeTestBridge.h"

#include "HAL/PlatformAtomics.h"
#include "Misc/ScopeRWLock.h"
#include "Containers/Queue.h"

namespace
{
	FRWLock GSmokeTestSinkLock;
	TSharedPtr<ITimeThiefSmokeTestSink, ESPMode::ThreadSafe> GSmokeTestSink;
	FString GSmokeTestPhase;
	TAtomic<bool> GSmokeTestMeasurementActive(false);
	TAtomic<uint64> GSmokeTestProbeRequestId(0);
	TAtomic<int32> GSmokeTestPendingGpuQueries(0);
	TAtomic<int32> GSmokeTestPendingProbes(0);
	TQueue<FTimeThiefSmokeTestProbeRequest, EQueueMode::Mpsc> GSmokeTestProbeRequests;

	TSharedPtr<ITimeThiefSmokeTestSink, ESPMode::ThreadSafe> GetSink()
	{
		FReadScopeLock Lock(GSmokeTestSinkLock);
		return GSmokeTestSink;
	}
}

bool FTimeThiefSmokeTestBridge::IsActive()
{
#if UE_BUILD_SHIPPING
	return false;
#else
	FReadScopeLock Lock(GSmokeTestSinkLock);
	return GSmokeTestSink.IsValid();
#endif
}

void FTimeThiefSmokeTestBridge::SetSink(TSharedPtr<ITimeThiefSmokeTestSink, ESPMode::ThreadSafe> Sink)
{
#if !UE_BUILD_SHIPPING
	FWriteScopeLock Lock(GSmokeTestSinkLock);
	GSmokeTestSink = MoveTemp(Sink);
#endif
}

void FTimeThiefSmokeTestBridge::ClearSink()
{
#if !UE_BUILD_SHIPPING
	FWriteScopeLock Lock(GSmokeTestSinkLock);
	GSmokeTestSink.Reset();
	GSmokeTestPhase.Reset();
	GSmokeTestMeasurementActive.Store(false);
	GSmokeTestPendingGpuQueries.Store(0);
	GSmokeTestPendingProbes.Store(0);
	FTimeThiefSmokeTestProbeRequest Request;
	while (GSmokeTestProbeRequests.Dequeue(Request))
	{
	}
#endif
}

void FTimeThiefSmokeTestBridge::Emit(const FTimeThiefSmokeTestEvent& Event)
{
#if !UE_BUILD_SHIPPING
	if (TSharedPtr<ITimeThiefSmokeTestSink, ESPMode::ThreadSafe> Sink = GetSink())
	{
		Sink->EnqueueEvent(Event);
	}
#endif
}

void FTimeThiefSmokeTestBridge::EmitGpuPass(const FTimeThiefSmokeTestGpuPassResult& Result)
{
#if !UE_BUILD_SHIPPING
	if (TSharedPtr<ITimeThiefSmokeTestSink, ESPMode::ThreadSafe> Sink = GetSink())
	{
		Sink->EnqueueGpuPass(Result);
	}
#endif
}

void FTimeThiefSmokeTestBridge::EmitProbe(const FTimeThiefSmokeTestProbeResult& Result)
{
#if !UE_BUILD_SHIPPING
	if (TSharedPtr<ITimeThiefSmokeTestSink, ESPMode::ThreadSafe> Sink = GetSink())
	{
		Sink->EnqueueProbe(Result);
	}
	NotifyProbeFinished();
#endif
}

void FTimeThiefSmokeTestBridge::SetPhase(const FString& Phase)
{
#if !UE_BUILD_SHIPPING
	FWriteScopeLock Lock(GSmokeTestSinkLock);
	GSmokeTestPhase = Phase;
#endif
}

FString FTimeThiefSmokeTestBridge::GetPhase()
{
#if UE_BUILD_SHIPPING
	return FString();
#else
	FReadScopeLock Lock(GSmokeTestSinkLock);
	return GSmokeTestPhase;
#endif
}

void FTimeThiefSmokeTestBridge::SetMeasurementActive(bool bActive)
{
#if !UE_BUILD_SHIPPING
	GSmokeTestMeasurementActive.Store(bActive);
#endif
}

bool FTimeThiefSmokeTestBridge::IsMeasurementActive()
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return IsActive() && GSmokeTestMeasurementActive.Load();
#endif
}

uint64 FTimeThiefSmokeTestBridge::RequestProbe(const FString& Label, const TArray<int32>& SmokeIds)
{
#if UE_BUILD_SHIPPING
	return 0;
#else
	FTimeThiefSmokeTestProbeRequest Request;
	Request.RequestId = ++GSmokeTestProbeRequestId;
	Request.Label = Label;
	Request.SmokeIds = SmokeIds;
	GSmokeTestPendingProbes += SmokeIds.Num();
	GSmokeTestProbeRequests.Enqueue(MoveTemp(Request));
	return GSmokeTestProbeRequestId.Load();
#endif
}

void FTimeThiefSmokeTestBridge::NotifyGpuQueryQueued()
{
#if !UE_BUILD_SHIPPING
	++GSmokeTestPendingGpuQueries;
#endif
}

void FTimeThiefSmokeTestBridge::NotifyGpuQueryFinished()
{
#if !UE_BUILD_SHIPPING
	if (GSmokeTestPendingGpuQueries.Load() > 0) --GSmokeTestPendingGpuQueries;
#endif
}

int32 FTimeThiefSmokeTestBridge::GetPendingGpuQueryCount()
{
#if UE_BUILD_SHIPPING
	return 0;
#else
	return GSmokeTestPendingGpuQueries.Load();
#endif
}

void FTimeThiefSmokeTestBridge::NotifyProbeFinished()
{
#if !UE_BUILD_SHIPPING
	if (GSmokeTestPendingProbes.Load() > 0) --GSmokeTestPendingProbes;
#endif
}

int32 FTimeThiefSmokeTestBridge::GetPendingProbeCount()
{
#if UE_BUILD_SHIPPING
	return 0;
#else
	return GSmokeTestPendingProbes.Load();
#endif
}

bool FTimeThiefSmokeTestBridge::DequeueProbeRequest(FTimeThiefSmokeTestProbeRequest& OutRequest)
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return GSmokeTestProbeRequests.Dequeue(OutRequest);
#endif
}
