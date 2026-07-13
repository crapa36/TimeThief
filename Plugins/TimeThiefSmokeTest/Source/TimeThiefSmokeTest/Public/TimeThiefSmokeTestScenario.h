#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

enum class ETimeThiefSmokeTestActionType : uint8
{
	DetonateSmoke,
	FireBullet,
	FireBullets,
	Explode,
	SpawnMover,
	SpawnObstacle,
	DestroyEntity,
	MoveCamera,
	Wait,
	SetPhase,
	StartMeasurement,
	StopMeasurement,
	CaptureProbe,
	CaptureScreenshot
};

struct TIMETHIEFSMOKETEST_API FTimeThiefSmokeTestAction
{
	FString Id;
	double TimeSeconds = 0.0;
	ETimeThiefSmokeTestActionType Type = ETimeThiefSmokeTestActionType::Wait;
	TSharedPtr<FJsonObject> Parameters;
	int32 SourceIndex = INDEX_NONE;
};

struct TIMETHIEFSMOKETEST_API FTimeThiefSmokeTestScenario
{
	FString Name;
	int32 Seed = 0;
	double WarmupSeconds = 0.0;
	double DurationSeconds = 0.0;
	TArray<FTimeThiefSmokeTestAction> Actions;
	TSharedPtr<FJsonObject> Expectations;
};

class TIMETHIEFSMOKETEST_API FTimeThiefSmokeTestScenarioParser
{
public:
	static bool LoadFile(const FString& Path, FTimeThiefSmokeTestScenario& OutScenario, FString& OutError);
	static bool ReadVector(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, FVector& OutVector, FString& OutError);
};
