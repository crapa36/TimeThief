#include "TimeThiefSmokeTestScenario.h"

#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	bool ParseActionType(const FString& Text, ETimeThiefSmokeTestActionType& OutType)
	{
		static const TMap<FString, ETimeThiefSmokeTestActionType> Types = {
			{ TEXT("detonate_smoke"), ETimeThiefSmokeTestActionType::DetonateSmoke },
			{ TEXT("fire_bullet"), ETimeThiefSmokeTestActionType::FireBullet },
			{ TEXT("fire_bullets"), ETimeThiefSmokeTestActionType::FireBullets },
			{ TEXT("explode"), ETimeThiefSmokeTestActionType::Explode },
			{ TEXT("spawn_mover"), ETimeThiefSmokeTestActionType::SpawnMover },
			{ TEXT("spawn_obstacle"), ETimeThiefSmokeTestActionType::SpawnObstacle },
			{ TEXT("destroy_entity"), ETimeThiefSmokeTestActionType::DestroyEntity },
			{ TEXT("move_camera"), ETimeThiefSmokeTestActionType::MoveCamera },
			{ TEXT("wait"), ETimeThiefSmokeTestActionType::Wait },
			{ TEXT("set_phase"), ETimeThiefSmokeTestActionType::SetPhase },
			{ TEXT("start_measurement"), ETimeThiefSmokeTestActionType::StartMeasurement },
			{ TEXT("stop_measurement"), ETimeThiefSmokeTestActionType::StopMeasurement },
			{ TEXT("capture_probe"), ETimeThiefSmokeTestActionType::CaptureProbe },
			{ TEXT("capture_screenshot"), ETimeThiefSmokeTestActionType::CaptureScreenshot }
		};

		if (const ETimeThiefSmokeTestActionType* Type = Types.Find(Text))
		{
			OutType = *Type;
			return true;
		}
		return false;
	}
}

bool FTimeThiefSmokeTestScenarioParser::LoadFile(const FString& Path, FTimeThiefSmokeTestScenario& OutScenario, FString& OutError)
{
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *Path))
	{
		OutError = FString::Printf(TEXT("Cannot read scenario file: %s"), *Path);
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = FString::Printf(TEXT("Invalid scenario JSON: %s"), *Reader->GetErrorMessage());
		return false;
	}

	if (!Root->TryGetStringField(TEXT("name"), OutScenario.Name) || OutScenario.Name.IsEmpty())
	{
		OutError = TEXT("Scenario field 'name' must be a non-empty string");
		return false;
	}

	double Seed = 0.0;
	Root->TryGetNumberField(TEXT("seed"), Seed);
	OutScenario.Seed = static_cast<int32>(Seed);
	Root->TryGetNumberField(TEXT("warmup_seconds"), OutScenario.WarmupSeconds);
	Root->TryGetNumberField(TEXT("duration_seconds"), OutScenario.DurationSeconds);
	if (OutScenario.WarmupSeconds < 0.0 || OutScenario.DurationSeconds <= 0.0)
	{
		OutError = TEXT("warmup_seconds must be non-negative and duration_seconds must be positive");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* ActionValues = nullptr;
	if (!Root->TryGetArrayField(TEXT("actions"), ActionValues))
	{
		OutError = TEXT("Scenario field 'actions' must be an array");
		return false;
	}

	TSet<FString> ActionIds;
	OutScenario.Actions.Reset(ActionValues->Num());
	for (int32 Index = 0; Index < ActionValues->Num(); ++Index)
	{
		const TSharedPtr<FJsonObject> ActionObject = (*ActionValues)[Index]->AsObject();
		if (!ActionObject.IsValid())
		{
			OutError = FString::Printf(TEXT("actions[%d] must be an object"), Index);
			return false;
		}

		FTimeThiefSmokeTestAction Action;
		FString TypeText;
		Action.SourceIndex = Index;
		Action.Parameters = ActionObject;
		if (!ActionObject->TryGetStringField(TEXT("id"), Action.Id) || Action.Id.IsEmpty() || ActionIds.Contains(Action.Id))
		{
			OutError = FString::Printf(TEXT("actions[%d].id must be non-empty and unique"), Index);
			return false;
		}
		if (!ActionObject->TryGetNumberField(TEXT("time"), Action.TimeSeconds) || Action.TimeSeconds < 0.0)
		{
			OutError = FString::Printf(TEXT("actions[%d].time must be non-negative"), Index);
			return false;
		}
		if (!ActionObject->TryGetStringField(TEXT("type"), TypeText) || !ParseActionType(TypeText, Action.Type))
		{
			OutError = FString::Printf(TEXT("actions[%d].type is unsupported: %s"), Index, *TypeText);
			return false;
		}

		ActionIds.Add(Action.Id);
		OutScenario.Actions.Add(MoveTemp(Action));
	}

	OutScenario.Actions.StableSort([](const FTimeThiefSmokeTestAction& Left, const FTimeThiefSmokeTestAction& Right)
	{
		return Left.TimeSeconds < Right.TimeSeconds;
	});

	const TSharedPtr<FJsonObject>* Expectations = nullptr;
	if (Root->TryGetObjectField(TEXT("expectations"), Expectations) && Expectations)
	{
		OutScenario.Expectations = *Expectations;
	}
	else
	{
		OutScenario.Expectations = MakeShared<FJsonObject>();
	}
	return true;
}

bool FTimeThiefSmokeTestScenarioParser::ReadVector(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, FVector& OutVector, FString& OutError)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Object.IsValid() || !Object->TryGetArrayField(Field, Values) || Values->Num() != 3)
	{
		OutError = FString::Printf(TEXT("Field '%s' must be a three-number array"), Field);
		return false;
	}

	double X = 0.0;
	double Y = 0.0;
	double Z = 0.0;
	if (!(*Values)[0]->TryGetNumber(X) || !(*Values)[1]->TryGetNumber(Y) || !(*Values)[2]->TryGetNumber(Z))
	{
		OutError = FString::Printf(TEXT("Field '%s' must contain only numbers"), Field);
		return false;
	}
	OutVector = FVector(X, Y, Z);
	return true;
}
