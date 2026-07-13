#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TimeThiefSmokeTestScenario.h"
#include "TimeThiefSmokeTestWorldSubsystem.generated.h"

class ACameraActor;
class ATimeThiefSmokeVolume;
class FTimeThiefSmokeTestRecorder;

enum class ETimeThiefSmokeTestState : uint8
{
	Inactive,
	Loading,
	WarmingUp,
	Executing,
	WaitingForGpu,
	WritingResult,
	Complete,
	Failed
};

UCLASS()
class TIMETHIEFSMOKETEST_API UTimeThiefSmokeTestWorldSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

private:
	bool LoadScenarioFromCommandLine(FString& OutError);
	void StartScenario();
	void ExecuteReadyActions();
	bool ExecuteAction(const FTimeThiefSmokeTestAction& Action, FString& OutError);
	void FinishScenario();
	void FailBeforeStart(const FString& Error);
	void EmitActionEvent(const TCHAR* Type, const FTimeThiefSmokeTestAction& Action, const FString& Detail = FString()) const;

	ETimeThiefSmokeTestState State = ETimeThiefSmokeTestState::Inactive;
	FTimeThiefSmokeTestScenario Scenario;
	TSharedPtr<FTimeThiefSmokeTestRecorder, ESPMode::ThreadSafe> Recorder;
	TMap<FString, TWeakObjectPtr<AActor>> Entities;
	TArray<FString> ExecutionErrors;
	FString ScenarioPath;
	FString OutputDirectory;
	double StateTimeSeconds = 0.0;
	double ScenarioTimeSeconds = 0.0;
	int32 NextActionIndex = 0;
	bool bAutoQuit = false;
	bool bMeasurementWasRequested = false;
};
