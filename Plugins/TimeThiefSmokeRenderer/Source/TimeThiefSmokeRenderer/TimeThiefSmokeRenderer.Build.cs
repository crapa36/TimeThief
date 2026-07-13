using UnrealBuildTool;

public class TimeThiefSmokeRenderer : ModuleRules
{
	public TimeThiefSmokeRenderer(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateIncludePaths.Add(EngineDirectory + "/Source/Runtime/Renderer/Private");

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Projects",
			"RenderCore",
			"Renderer",
			"RHI"
		});
	}
}
