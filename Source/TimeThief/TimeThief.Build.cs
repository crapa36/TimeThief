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
			"CableComponent",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"SlateCore",
            "MotionTrajectory",
            "PoseSearch",
            "GameplayTags",
            "ModularGameplay",
            "GameFeatures",
            "Json",
            "JsonUtilities",
            "Niagara",
            "Protobuf",
            "ProtocolSharedUE",
            "AnimGraphRuntime",
            "DeveloperSettings",
            "TimeThiefSmokeRenderer",
        });

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Sockets",
			"Networking", "MorphingMesh",
		});

		PublicIncludePaths.AddRange(new string[] {
			"TimeThief"
		});
		
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UnrealEd"
				}
			);
		}
    }
}
