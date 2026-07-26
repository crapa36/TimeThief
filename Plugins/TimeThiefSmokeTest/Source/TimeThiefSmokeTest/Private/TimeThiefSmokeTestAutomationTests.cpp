#include "TimeThiefSmokeTestScenario.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTimeThiefSmokeTestScenarioParserAutomationTest,
	"TimeThief.SmokeTest.ScenarioParser",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTimeThiefSmokeTestScenarioParserAutomationTest::RunTest(const FString& Parameters)
{
	const FString ValidPath = FPaths::CreateTempFilename(*FPaths::ProjectIntermediateDir(), TEXT("SmokeScenarioValid"), TEXT(".json"));
	const FString ValidJson = TEXT(R"JSON({
		"name":"StableOrder",
		"seed":42,
		"warmup_seconds":0,
		"duration_seconds":2,
		"actions":[
			{"id":"B","time":1,"type":"wait"},
			{"id":"A","time":1,"type":"wait"},
			{"id":"Early","time":0,"type":"wait"}
		],
		"expectations":{}
	})JSON");
	TestTrue(TEXT("writes valid fixture"), FFileHelper::SaveStringToFile(ValidJson, *ValidPath));

	FTimeThiefSmokeTestScenario Scenario;
	FString Error;
	TestTrue(TEXT("valid scenario parses"), FTimeThiefSmokeTestScenarioParser::LoadFile(ValidPath, Scenario, Error));
	TestEqual(TEXT("action count"), Scenario.Actions.Num(), 3);
	if (Scenario.Actions.Num() == 3)
	{
		TestEqual(TEXT("earlier action sorts first"), Scenario.Actions[0].Id, FString(TEXT("Early")));
		TestEqual(TEXT("same-time action preserves JSON order (first)"), Scenario.Actions[1].Id, FString(TEXT("B")));
		TestEqual(TEXT("same-time action preserves JSON order (second)"), Scenario.Actions[2].Id, FString(TEXT("A")));
	}
	IFileManager::Get().Delete(*ValidPath);

	const FString InvalidPath = FPaths::CreateTempFilename(*FPaths::ProjectIntermediateDir(), TEXT("SmokeScenarioInvalid"), TEXT(".json"));
	const FString InvalidJson = TEXT(R"JSON({
		"name":"DuplicateIds",
		"duration_seconds":1,
		"actions":[
			{"id":"same","time":0,"type":"wait"},
			{"id":"same","time":0,"type":"wait"}
		],
		"expectations":{}
	})JSON");
	TestTrue(TEXT("writes invalid fixture"), FFileHelper::SaveStringToFile(InvalidJson, *InvalidPath));
	Error.Reset();
	TestFalse(TEXT("duplicate action IDs are rejected"), FTimeThiefSmokeTestScenarioParser::LoadFile(InvalidPath, Scenario, Error));
	TestTrue(TEXT("duplicate rejection explains id"), Error.Contains(TEXT("id")));
	IFileManager::Get().Delete(*InvalidPath);
	return true;
}

#endif
