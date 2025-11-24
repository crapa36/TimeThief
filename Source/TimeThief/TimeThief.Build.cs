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
        });

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"TimeThief",
			"TimeThief/Variant_Platforming",
			"TimeThief/Variant_Platforming/Animation",
			"TimeThief/Variant_Combat",
			"TimeThief/Variant_Combat/AI",
			"TimeThief/Variant_Combat/Animation",
			"TimeThief/Variant_Combat/Gameplay",
			"TimeThief/Variant_Combat/Interfaces",
			"TimeThief/Variant_Combat/UI",
			"TimeThief/Variant_SideScrolling",
			"TimeThief/Variant_SideScrolling/AI",
			"TimeThief/Variant_SideScrolling/Gameplay",
			"TimeThief/Variant_SideScrolling/Interfaces",
			"TimeThief/Variant_SideScrolling/UI"
		});

        PrivateDependencyModuleNames.AddRange(new string[] {
            "GameplayAbilities",
            "GameplayTags",
            "GameplayTasks"
        });

        // Uncomment if you are using Slate UI
        // PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

        // Uncomment if you are using online features
        // PrivateDependencyModuleNames.Add("OnlineSubsystem");

        // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
    }
}
