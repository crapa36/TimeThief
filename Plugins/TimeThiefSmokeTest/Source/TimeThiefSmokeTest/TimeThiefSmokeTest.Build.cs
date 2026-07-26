using UnrealBuildTool;

public class TimeThiefSmokeTest : ModuleRules
{
	public TimeThiefSmokeTest(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Json",
			"TimeThief",
			"TimeThiefSmokeRenderer"
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"JsonUtilities"
		});
	}
}
