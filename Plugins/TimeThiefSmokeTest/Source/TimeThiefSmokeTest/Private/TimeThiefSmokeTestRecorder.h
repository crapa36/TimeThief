#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "TimeThiefSmokeTestBridge.h"
#include "TimeThiefSmokeTestResult.h"

struct FTimeThiefSmokeTestScenario;

struct FTimeThiefSmokeTestPhasePassSamples
{
	TArray<double> Durations;
	TArray<double> DrawFractions;
	TArray<double> CameraInsideCounts;
	TArray<double> SurfaceDistances;
};

class FTimeThiefSmokeTestRecorder final : public ITimeThiefSmokeTestSink
{
public:
	virtual ~FTimeThiefSmokeTestRecorder() override;

	bool Initialize(const FString& InOutputDirectory, FString& OutError);
	void SetScenarioLoaded(int32 ActionCount);
	void EnqueueEvent(const FTimeThiefSmokeTestEvent& Event) override;
	void EnqueueGpuPass(const FTimeThiefSmokeTestGpuPassResult& Result) override;
	void EnqueueProbe(const FTimeThiefSmokeTestProbeResult& Result) override;
	void Drain(double ScenarioTimeSeconds);
	bool IsEmpty() const;
	bool WriteResult(const FTimeThiefSmokeTestScenario& Scenario, const TArray<FString>& ExecutionErrors, FString& OutError);

	const FTimeThiefSmokeTestExecutionSummary& GetExecution() const { return Execution; }
	const FTimeThiefSmokeTestSituationSummary& GetSituation() const { return Situation; }

private:
	enum class ERecordType : uint8 { Event, GpuPass, Probe };
	struct FPendingRecord
	{
		ERecordType Type = ERecordType::Event;
		FTimeThiefSmokeTestEvent Event;
		FTimeThiefSmokeTestGpuPassResult GpuPass;
		FTimeThiefSmokeTestProbeResult Probe;
	};

	void WriteEvent(const FTimeThiefSmokeTestEvent& Event, double TimeSeconds);
	void WriteGpuPass(const FTimeThiefSmokeTestGpuPassResult& Result, double TimeSeconds);
	void WriteProbe(const FTimeThiefSmokeTestProbeResult& Result, double TimeSeconds);
	void WriteLine(FArchive* Archive, const FString& Line);

	TQueue<FPendingRecord, EQueueMode::Mpsc> PendingRecords;
	TUniquePtr<FArchive> EventsArchive;
	TUniquePtr<FArchive> GpuArchive;
	TUniquePtr<FArchive> ProbesArchive;
	TUniquePtr<FArchive> TimelineArchive;
	FString OutputDirectory;
	uint64 Sequence = 0;
	FTimeThiefSmokeTestExecutionSummary Execution;
	FTimeThiefSmokeTestSituationSummary Situation;
	TMap<FString, TArray<double>> PassDurations;
	TMap<FString, TMap<FString, FTimeThiefSmokeTestPhasePassSamples>> PhasePassSamples;
	TMap<uint64, double> PressureDurationByFrameAndSmoke;
	TSet<FString> ExecutedPasses;
	TMap<FString, TMap<int32, FTimeThiefSmokeTestProbeResult>> ProbeResults;
	TSet<int32> RegisteredSmokeIds;
	TSet<int32> RenderedSmokeIds;
	int32 DuplicateRenderedSmokeIds = 0;
};
