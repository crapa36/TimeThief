// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
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
            "Protobuf",
            "ProtocolSharedUE",
            "AnimGraphRuntime",
            "DeveloperSettings",
        });

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Sockets",
			"Networking",
			"Niagara",
			"MorphingMesh",
		});

		if (IsPluginAvailable("DLSS") && IsPluginAvailable("StreamlineDLSSG"))
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"DLSSBlueprint",
				"StreamlineBlueprint",
				"StreamlineDLSSGBlueprint",
				"StreamlineReflexBlueprint",
			});

			PublicDefinitions.Add("TIMETHIEF_WITH_NVIDIA_DLSS=1");
		}
		else
		{
			PublicDefinitions.Add("TIMETHIEF_WITH_NVIDIA_DLSS=0");
		}

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

	private bool IsPluginAvailable(string PluginName)
	{
		string ProjectRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", ".."));
		string PluginFileName = PluginName + ".uplugin";

		return ContainsPlugin(Path.Combine(ProjectRoot, "Plugins"), PluginFileName)
			|| ContainsPlugin(Path.Combine(EngineDirectory, "Plugins"), PluginFileName);
	}

	private bool ContainsPlugin(string PluginRoot, string PluginFileName)
	{
		if (!Directory.Exists(PluginRoot))
		{
			return false;
		}

		return Directory.GetFiles(PluginRoot, PluginFileName, SearchOption.AllDirectories).Length > 0;
	}
}
