using UnrealBuildTool;

public class TimeThiefSmokeRenderer : ModuleRules
{
	public TimeThiefSmokeRenderer(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

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
