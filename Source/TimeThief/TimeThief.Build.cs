// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TimeThief : ModuleRules
{
	public TimeThief(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
            "MotionTrajectory",
            "PoseSearch",
            "GameplayTags",
            "ModularGameplay",
            "GameFeatures",
            "Protobuf"
        });

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Sockets",
			"Networking",
		});

		PublicIncludePaths.AddRange(new string[] {
			"TimeThief",
		});
    }
}
