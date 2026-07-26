#include "TimeThiefSmokeTestWorldSubsystem.h"

#include "Actors/TimeThiefSmokeVolume.h"
#include "Camera/CameraActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Smoke/TimeThiefSmokeWorldSubsystem.h"
#include "TimeThiefSmokeTestBridge.h"
#include "TimeThiefSmokeTestMover.h"
#include "TimeThiefSmokeTestObstacle.h"
#include "TimeThiefSmokeTestRecorder.h"
#include "UnrealClient.h"

namespace
{
	bool ReadOptionalVector(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, const FVector& Default, FVector& OutValue, FString& OutError)
	{
		if (!Object->HasField(Field))
		{
			OutValue = Default;
			return true;
		}
		return FTimeThiefSmokeTestScenarioParser::ReadVector(Object, Field, OutValue, OutError);
	}

	float ReadFloat(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, float Default)
	{
		double Value = Default;
		Object->TryGetNumberField(Field, Value);
		return static_cast<float>(Value);
	}

	int32 ReadInt(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, int32 Default)
	{
		double Value = Default;
		Object->TryGetNumberField(Field, Value);
		return static_cast<int32>(Value);
	}

	FString ReadString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, const TCHAR* Default = TEXT(""))
	{
		FString Value = Default;
		Object->TryGetStringField(Field, Value);
		return Value;
	}

}

bool UTimeThiefSmokeTestWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	FString Ignored;
	return Super::ShouldCreateSubsystem(Outer) && FParse::Value(FCommandLine::Get(), TEXT("SmokeTestScenario="), Ignored);
}

void UTimeThiefSmokeTestWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	State = ETimeThiefSmokeTestState::Loading;
	bAutoQuit = FParse::Param(FCommandLine::Get(), TEXT("SmokeTestAutoQuit"));

	FString Error;
	if (!LoadScenarioFromCommandLine(Error))
	{
		FailBeforeStart(Error);
		return;
	}
	StartScenario();
}

void UTimeThiefSmokeTestWorldSubsystem::Deinitialize()
{
	FTimeThiefSmokeTestBridge::ClearSink();
	Recorder.Reset();
	Entities.Reset();
	Super::Deinitialize();
}

bool UTimeThiefSmokeTestWorldSubsystem::LoadScenarioFromCommandLine(FString& OutError)
{
	if (!FParse::Value(FCommandLine::Get(), TEXT("SmokeTestScenario="), ScenarioPath) || ScenarioPath.IsEmpty())
	{
		OutError = TEXT("Missing -SmokeTestScenario=<path>");
		return false;
	}
	ScenarioPath = FPaths::ConvertRelativePathToFull(ScenarioPath);
	if (!FTimeThiefSmokeTestScenarioParser::LoadFile(ScenarioPath, Scenario, OutError))
	{
		return false;
	}

	if (!FParse::Value(FCommandLine::Get(), TEXT("SmokeTestOutput="), OutputDirectory) || OutputDirectory.IsEmpty())
	{
		OutputDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SmokeTests"), Scenario.Name);
	}
	OutputDirectory = FPaths::ConvertRelativePathToFull(OutputDirectory);
	return true;
}

void UTimeThiefSmokeTestWorldSubsystem::StartScenario()
{
	Recorder = MakeShared<FTimeThiefSmokeTestRecorder, ESPMode::ThreadSafe>();
	FString Error;
	if (!Recorder->Initialize(OutputDirectory, Error))
	{
		FailBeforeStart(Error);
		return;
	}
	Recorder->SetScenarioLoaded(Scenario.Actions.Num());
	FTimeThiefSmokeTestBridge::SetSink(Recorder);
	FTimeThiefSmokeTestBridge::SetPhase(TEXT("warmup"));
	FTimeThiefSmokeTestBridge::SetMeasurementActive(false);

	FTimeThiefSmokeTestEvent Event;
	Event.Type = TEXT("scenario_started");
	Event.Label = Scenario.Name;
	Event.Seed = Scenario.Seed;
	Event.FrameId = GFrameCounter;
	FTimeThiefSmokeTestBridge::Emit(Event);
	StateTimeSeconds = 0.0;
	ScenarioTimeSeconds = 0.0;
	State = Scenario.WarmupSeconds > 0.0 ? ETimeThiefSmokeTestState::WarmingUp : ETimeThiefSmokeTestState::Executing;
}

void UTimeThiefSmokeTestWorldSubsystem::Tick(float DeltaSeconds)
{
	if (!Recorder.IsValid())
	{
		return;
	}

	Recorder->Drain(ScenarioTimeSeconds);
	StateTimeSeconds += FMath::Max(0.0f, DeltaSeconds);
	switch (State)
	{
	case ETimeThiefSmokeTestState::WarmingUp:
		if (StateTimeSeconds >= Scenario.WarmupSeconds)
		{
			State = ETimeThiefSmokeTestState::Executing;
			StateTimeSeconds = 0.0;
			FTimeThiefSmokeTestBridge::SetPhase(TEXT("execution"));
		}
		break;

	case ETimeThiefSmokeTestState::Executing:
		ScenarioTimeSeconds += FMath::Max(0.0f, DeltaSeconds);
		ExecuteReadyActions();
		if (ScenarioTimeSeconds >= Scenario.DurationSeconds)
		{
			State = ETimeThiefSmokeTestState::WaitingForGpu;
			StateTimeSeconds = 0.0;
			FTimeThiefSmokeTestBridge::SetMeasurementActive(false);
		}
		break;

	case ETimeThiefSmokeTestState::WaitingForGpu:
		if (StateTimeSeconds >= 0.25 &&
			FTimeThiefSmokeTestBridge::GetPendingGpuQueryCount() == 0 &&
			FTimeThiefSmokeTestBridge::GetPendingProbeCount() == 0 &&
			Recorder->IsEmpty())
		{
			FinishScenario();
		}
		else if (StateTimeSeconds >= 5.0)
		{
			if (FTimeThiefSmokeTestBridge::GetPendingGpuQueryCount() > 0) ExecutionErrors.Add(TEXT("gpu_query_incomplete"));
			if (FTimeThiefSmokeTestBridge::GetPendingProbeCount() > 0) ExecutionErrors.Add(TEXT("probe_readback_incomplete"));
			FinishScenario();
		}
		break;

	default:
		break;
	}
}

bool UTimeThiefSmokeTestWorldSubsystem::IsTickable() const
{
	return State == ETimeThiefSmokeTestState::WarmingUp ||
		State == ETimeThiefSmokeTestState::Executing ||
		State == ETimeThiefSmokeTestState::WaitingForGpu;
}

TStatId UTimeThiefSmokeTestWorldSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTimeThiefSmokeTestWorldSubsystem, STATGROUP_Tickables);
}

void UTimeThiefSmokeTestWorldSubsystem::ExecuteReadyActions()
{
	while (Scenario.Actions.IsValidIndex(NextActionIndex) && Scenario.Actions[NextActionIndex].TimeSeconds <= ScenarioTimeSeconds)
	{
		const FTimeThiefSmokeTestAction& Action = Scenario.Actions[NextActionIndex++];
		EmitActionEvent(TEXT("action_started"), Action);
		FString Error;
		if (ExecuteAction(Action, Error))
		{
			EmitActionEvent(TEXT("action_completed"), Action);
		}
		else
		{
			ExecutionErrors.Add(FString::Printf(TEXT("action %s: %s"), *Action.Id, *Error));
			EmitActionEvent(TEXT("action_failed"), Action, Error);
		}
	}
}

bool UTimeThiefSmokeTestWorldSubsystem::ExecuteAction(const FTimeThiefSmokeTestAction& Action, FString& OutError)
{
	UWorld* World = GetWorld();
	UTimeThiefSmokeWorldSubsystem* SmokeSubsystem = World ? World->GetSubsystem<UTimeThiefSmokeWorldSubsystem>() : nullptr;
	if (!World || !SmokeSubsystem)
	{
		OutError = TEXT("Smoke world subsystem is unavailable");
		return false;
	}

	const TSharedPtr<FJsonObject>& P = Action.Parameters;
	switch (Action.Type)
	{
	case ETimeThiefSmokeTestActionType::DetonateSmoke:
	{
		FVector Position;
		if (!FTimeThiefSmokeTestScenarioParser::ReadVector(P, TEXT("position"), Position, OutError)) return false;
		const FString EntityId = ReadString(P, TEXT("entity"));
		if (EntityId.IsEmpty() || Entities.Contains(EntityId))
		{
			OutError = TEXT("detonate_smoke requires a unique non-empty entity");
			return false;
		}
		FTimeThiefSmokeTestEvent Detonated;
		Detonated.Type = TEXT("smoke_detonated");
		Detonated.ActionId = Action.Id;
		Detonated.EntityId = EntityId;
		Detonated.Position = Position;
		Detonated.FrameId = GFrameCounter;
		FTimeThiefSmokeTestBridge::Emit(Detonated);

		const FTransform SpawnTransform(FRotator::ZeroRotator, Position);
		ATimeThiefSmokeVolume* Smoke = World->SpawnActorDeferred<ATimeThiefSmokeVolume>(
			ATimeThiefSmokeVolume::StaticClass(), SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Smoke)
		{
			OutError = TEXT("Failed to spawn ATimeThiefSmokeVolume");
			return false;
		}
		Smoke->InitializeSmokeVolume(nullptr, nullptr);
		UGameplayStatics::FinishSpawningActor(Smoke, SpawnTransform);
		if (Smoke->GetSmokeId() == INDEX_NONE)
		{
			Smoke->Destroy();
			OutError = TEXT("Spawned smoke has no SmokeId");
			return false;
		}
		Entities.Add(EntityId, Smoke);
		return true;
	}

	case ETimeThiefSmokeTestActionType::FireBullet:
	{
		FVector Start;
		FVector End;
		if (!FTimeThiefSmokeTestScenarioParser::ReadVector(P, TEXT("start"), Start, OutError) ||
			!FTimeThiefSmokeTestScenarioParser::ReadVector(P, TEXT("end"), End, OutError)) return false;
		const float Strength = ReadFloat(P, TEXT("strength"), 1.0f);
		const int32 Seed = ReadInt(P, TEXT("seed"), Scenario.Seed);
		FTimeThiefSmokeTestEvent Fired;
		Fired.Type = TEXT("bullet_fired");
		Fired.ActionId = Action.Id;
		Fired.ItemIndex = 0;
		Fired.Start = Start;
		Fired.End = End;
		Fired.Strength = Strength;
		Fired.Seed = Seed;
		Fired.FrameId = GFrameCounter;
		FTimeThiefSmokeTestBridge::Emit(Fired);
		SmokeSubsystem->SubmitBulletTrace(Start, End, Strength, Seed);
		return true;
	}

	case ETimeThiefSmokeTestActionType::FireBullets:
	{
		FVector StartCenter;
		FVector EndCenter;
		FVector Spread;
		if (!FTimeThiefSmokeTestScenarioParser::ReadVector(P, TEXT("start_center"), StartCenter, OutError) ||
			!FTimeThiefSmokeTestScenarioParser::ReadVector(P, TEXT("end_center"), EndCenter, OutError) ||
			!ReadOptionalVector(P, TEXT("spread"), FVector::ZeroVector, Spread, OutError)) return false;
		const int32 Count = ReadInt(P, TEXT("count"), 1);
		const int32 Seed = ReadInt(P, TEXT("seed"), Scenario.Seed);
		const float Strength = ReadFloat(P, TEXT("strength"), 1.0f);
		const FString Pattern = ReadString(P, TEXT("pattern"), TEXT("parallel"));
		if (Count <= 0 || !TSet<FString>{ TEXT("parallel"), TEXT("crossing"), TEXT("radial"), TEXT("repeated"), TEXT("random_seeded") }.Contains(Pattern))
		{
			OutError = TEXT("fire_bullets count must be positive and pattern must be supported");
			return false;
		}
		FRandomStream Random(Seed ^ Scenario.Seed);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const float Unit = Count > 1 ? (2.0f * Index / (Count - 1) - 1.0f) : 0.0f;
			FVector StartOffset(Spread.X * Unit, Spread.Y * Unit, Spread.Z * Unit);
			FVector EndOffset = StartOffset;
			if (Pattern == TEXT("crossing")) EndOffset = -StartOffset;
			else if (Pattern == TEXT("repeated")) StartOffset = EndOffset = FVector::ZeroVector;
			else if (Pattern == TEXT("radial"))
			{
				const float Angle = 2.0f * PI * Index / FMath::Max(1, Count);
				StartOffset = FVector::ZeroVector;
				EndOffset = FVector(Spread.X, Spread.Y * FMath::Cos(Angle), Spread.Z * FMath::Sin(Angle));
			}
			else if (Pattern == TEXT("random_seeded"))
			{
				StartOffset = FVector(Random.FRandRange(-Spread.X, Spread.X), Random.FRandRange(-Spread.Y, Spread.Y), Random.FRandRange(-Spread.Z, Spread.Z));
				EndOffset = FVector(Random.FRandRange(-Spread.X, Spread.X), Random.FRandRange(-Spread.Y, Spread.Y), Random.FRandRange(-Spread.Z, Spread.Z));
			}
			const FVector Start = StartCenter + StartOffset;
			const FVector End = EndCenter + EndOffset;
			FTimeThiefSmokeTestEvent Fired;
			Fired.Type = TEXT("bullet_fired");
			Fired.ActionId = Action.Id;
			Fired.ItemIndex = Index;
			Fired.Start = Start;
			Fired.End = End;
			Fired.Strength = Strength;
			Fired.Seed = Seed + Index;
			Fired.FrameId = GFrameCounter;
			FTimeThiefSmokeTestBridge::Emit(Fired);
			SmokeSubsystem->SubmitBulletTrace(Start, End, Strength, Seed + Index);
		}
		return true;
	}

	case ETimeThiefSmokeTestActionType::Explode:
	{
		FVector Position;
		if (!FTimeThiefSmokeTestScenarioParser::ReadVector(P, TEXT("position"), Position, OutError)) return false;
		SmokeSubsystem->SubmitExplosion(Position, ReadFloat(P, TEXT("radius"), 1.0f), ReadFloat(P, TEXT("strength"), 1.0f), ReadInt(P, TEXT("seed"), Scenario.Seed));
		return true;
	}

	case ETimeThiefSmokeTestActionType::SpawnMover:
	{
		const FString EntityId = ReadString(P, TEXT("entity"), *Action.Id);
		if (Entities.Contains(EntityId)) { OutError = TEXT("spawn_mover entity already exists"); return false; }
		FTimeThiefSmokeTestMoverSettings Settings;
		Settings.Shape = ReadString(P, TEXT("shape"), TEXT("capsule"));
		if (!FTimeThiefSmokeTestScenarioParser::ReadVector(P, TEXT("start"), Settings.Start, OutError) ||
			!FTimeThiefSmokeTestScenarioParser::ReadVector(P, TEXT("end"), Settings.End, OutError) ||
			!ReadOptionalVector(P, TEXT("extent"), FVector(50.0), Settings.Extent, OutError)) return false;
		Settings.Duration = ReadFloat(P, TEXT("duration"), 1.0f);
		Settings.Radius = ReadFloat(P, TEXT("radius"), 50.0f);
		Settings.HalfHeight = ReadFloat(P, TEXT("half_height"), 100.0f);
		if (!TSet<FString>{ TEXT("sphere"), TEXT("capsule"), TEXT("box") }.Contains(Settings.Shape) || Settings.Duration <= 0.0f)
		{
			OutError = TEXT("spawn_mover has invalid shape or duration"); return false;
		}
		ATimeThiefSmokeTestMover* Mover = World->SpawnActor<ATimeThiefSmokeTestMover>();
		if (!Mover) { OutError = TEXT("Failed to spawn mover"); return false; }
		Mover->Configure(Settings);
		Entities.Add(EntityId, Mover);
		FTimeThiefSmokeTestEvent Event;
		Event.Type = TEXT("mover_spawned"); Event.ActionId = Action.Id; Event.EntityId = EntityId; Event.Shape = Settings.Shape;
		Event.Start = Settings.Start; Event.End = Settings.End; Event.FrameId = GFrameCounter;
		FTimeThiefSmokeTestBridge::Emit(Event);
		return true;
	}

	case ETimeThiefSmokeTestActionType::SpawnObstacle:
	{
		const FString EntityId = ReadString(P, TEXT("entity"), *Action.Id);
		if (Entities.Contains(EntityId)) { OutError = TEXT("spawn_obstacle entity already exists"); return false; }
		FTimeThiefSmokeTestObstacleSettings Settings;
		Settings.Shape = ReadString(P, TEXT("shape"), TEXT("box"));
		if (!FTimeThiefSmokeTestScenarioParser::ReadVector(P, TEXT("position"), Settings.Position, OutError) ||
			!ReadOptionalVector(P, TEXT("extent"), FVector(50.0), Settings.Extent, OutError)) return false;
		Settings.Radius = ReadFloat(P, TEXT("radius"), 50.0f);
		Settings.HalfHeight = ReadFloat(P, TEXT("half_height"), 100.0f);
		if (!TSet<FString>{ TEXT("sphere"), TEXT("capsule"), TEXT("box") }.Contains(Settings.Shape))
		{
			OutError = TEXT("spawn_obstacle has invalid shape"); return false;
		}
		ATimeThiefSmokeTestObstacle* Obstacle = World->SpawnActor<ATimeThiefSmokeTestObstacle>(Settings.Position, FRotator::ZeroRotator);
		if (!Obstacle) { OutError = TEXT("Failed to spawn obstacle"); return false; }
		Obstacle->Configure(Settings);
		Entities.Add(EntityId, Obstacle);
		FTimeThiefSmokeTestEvent Event;
		Event.Type = TEXT("obstacle_spawned"); Event.ActionId = Action.Id; Event.EntityId = EntityId; Event.Shape = Settings.Shape;
		Event.Position = Settings.Position; Event.Extents = Settings.Extent; Event.FrameId = GFrameCounter;
		FTimeThiefSmokeTestBridge::Emit(Event);
		return true;
	}

	case ETimeThiefSmokeTestActionType::DestroyEntity:
	{
		const FString EntityId = ReadString(P, TEXT("entity"));
		TWeakObjectPtr<AActor>* ActorPtr = Entities.Find(EntityId);
		if (!ActorPtr || !ActorPtr->IsValid()) { OutError = FString::Printf(TEXT("Unknown entity: %s"), *EntityId); return false; }
		ActorPtr->Get()->Destroy();
		Entities.Remove(EntityId);
		return true;
	}

	case ETimeThiefSmokeTestActionType::MoveCamera:
	{
		FVector Position;
		FVector RotationVector;
		if (!FTimeThiefSmokeTestScenarioParser::ReadVector(P, TEXT("position"), Position, OutError) ||
			!ReadOptionalVector(P, TEXT("rotation"), FVector::ZeroVector, RotationVector, OutError)) return false;
		ACameraActor* Camera = Cast<ACameraActor>(Entities.FindRef(TEXT("__camera")).Get());
		if (!Camera)
		{
			Camera = World->SpawnActor<ACameraActor>();
			if (!Camera) { OutError = TEXT("Failed to spawn camera"); return false; }
			Entities.Add(TEXT("__camera"), Camera);
			if (APlayerController* Controller = UGameplayStatics::GetPlayerController(World, 0)) Controller->SetViewTarget(Camera);
		}
		Camera->SetActorLocationAndRotation(Position, FRotator(RotationVector.Y, RotationVector.Z, RotationVector.X));
		return true;
	}

	case ETimeThiefSmokeTestActionType::SetPhase:
	{
		const FString Phase = ReadString(P, TEXT("name"));
		if (Phase.IsEmpty()) { OutError = TEXT("set_phase requires name"); return false; }
		FTimeThiefSmokeTestBridge::SetPhase(Phase);
		return true;
	}

	case ETimeThiefSmokeTestActionType::StartMeasurement:
		bMeasurementWasRequested = true;
		FTimeThiefSmokeTestBridge::SetMeasurementActive(true);
		if (FParse::Param(FCommandLine::Get(), TEXT("SmokeTestCsvProfile")) && GEngine)
		{
			GEngine->Exec(GetWorld(), TEXT("csvprofile start"));
		}
		return true;

	case ETimeThiefSmokeTestActionType::StopMeasurement:
		FTimeThiefSmokeTestBridge::SetMeasurementActive(false);
		if (FParse::Param(FCommandLine::Get(), TEXT("SmokeTestCsvProfile")) && GEngine)
		{
			GEngine->Exec(GetWorld(), TEXT("csvprofile stop"));
		}
		if (FParse::Param(FCommandLine::Get(), TEXT("SmokeTestRhiMemoryDump")) && GEngine)
		{
			GEngine->Exec(GetWorld(), TEXT("rhi.DumpResourceMemory all Transient=all -csvfile"));
		}
		if (FParse::Param(FCommandLine::Get(), TEXT("SmokeTestDumpTicks")) && GEngine)
		{
			GEngine->Exec(GetWorld(), TEXT("dumpticks"));
		}
		return true;

	case ETimeThiefSmokeTestActionType::CaptureProbe:
	{
		const FString Label = ReadString(P, TEXT("label"));
		const TArray<TSharedPtr<FJsonValue>>* SmokeEntities = nullptr;
		if (Label.IsEmpty() || !P->TryGetArrayField(TEXT("smokes"), SmokeEntities))
		{
			OutError = TEXT("capture_probe requires label and smokes array"); return false;
		}
		TArray<int32> SmokeIds;
		for (const TSharedPtr<FJsonValue>& Value : *SmokeEntities)
		{
			const FString EntityId = Value->AsString();
			ATimeThiefSmokeVolume* Smoke = Cast<ATimeThiefSmokeVolume>(Entities.FindRef(EntityId).Get());
			if (!Smoke || Smoke->GetSmokeId() == INDEX_NONE)
			{
				OutError = FString::Printf(TEXT("capture_probe references unknown smoke: %s"), *EntityId); return false;
			}
			SmokeIds.Add(Smoke->GetSmokeId());
		}
		FTimeThiefSmokeTestEvent Event;
		Event.Type = TEXT("probe_requested"); Event.ActionId = Action.Id; Event.Label = Label; Event.SmokeIds = SmokeIds; Event.FrameId = GFrameCounter;
		FTimeThiefSmokeTestBridge::Emit(Event);
		FTimeThiefSmokeTestBridge::RequestProbe(Label, SmokeIds);
		return true;
	}

	case ETimeThiefSmokeTestActionType::CaptureScreenshot:
	{
		const FString Label = ReadString(P, TEXT("label"));
		if (Label.IsEmpty())
		{
			OutError = TEXT("capture_screenshot requires label");
			return false;
		}
		const FString ScreenshotPath = FPaths::Combine(OutputDirectory, FPaths::MakeValidFileName(Label) + TEXT(".png"));
		FScreenshotRequest::RequestScreenshot(ScreenshotPath, false, false);
		FTimeThiefSmokeTestEvent Event;
		Event.Type = TEXT("screenshot_requested");
		Event.ActionId = Action.Id;
		Event.Label = Label;
		Event.FrameId = GFrameCounter;
		FTimeThiefSmokeTestBridge::Emit(Event);
		return true;
	}

	case ETimeThiefSmokeTestActionType::Wait:
		return true;
	}
	OutError = TEXT("Unsupported action");
	return false;
}

void UTimeThiefSmokeTestWorldSubsystem::EmitActionEvent(const TCHAR* Type, const FTimeThiefSmokeTestAction& Action, const FString& Detail) const
{
	FTimeThiefSmokeTestEvent Event;
	Event.Type = Type;
	Event.ActionId = Action.Id;
	Event.Label = Detail;
	Event.FrameId = GFrameCounter;
	FTimeThiefSmokeTestBridge::Emit(Event);
}

void UTimeThiefSmokeTestWorldSubsystem::FinishScenario()
{
	FTimeThiefSmokeTestEvent Event;
	Event.Type = TEXT("scenario_finished");
	Event.Label = Scenario.Name;
	Event.FrameId = GFrameCounter;
	FTimeThiefSmokeTestBridge::Emit(Event);
	Recorder->Drain(ScenarioTimeSeconds);
	if (bMeasurementWasRequested && Recorder->GetExecution().GpuPassSamples == 0)
	{
		ExecutionErrors.Add(TEXT("gpu_measurement_requested_but_no_samples_received"));
	}
	FString Error;
	State = Recorder->WriteResult(Scenario, ExecutionErrors, Error) ? ETimeThiefSmokeTestState::Complete : ETimeThiefSmokeTestState::Failed;
	if (!Error.IsEmpty()) UE_LOG(LogTemp, Error, TEXT("TimeThief smoke test: %s"), *Error);
	FTimeThiefSmokeTestBridge::ClearSink();
	if (bAutoQuit) FPlatformMisc::RequestExit(false);
}

void UTimeThiefSmokeTestWorldSubsystem::FailBeforeStart(const FString& Error)
{
	ExecutionErrors.Add(Error);
	State = ETimeThiefSmokeTestState::Failed;
	UE_LOG(LogTemp, Error, TEXT("TimeThief smoke test failed to start: %s"), *Error);
	if (bAutoQuit) FPlatformMisc::RequestExit(false);
}
