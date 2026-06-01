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
			"Networking",
			"Niagara",
			"MorphingMesh",
		});

		bool bHasNvidiaDLSSModules = IsPluginAvailable("DLSS") && IsPluginAvailable("StreamlineDLSSG");
		if (bHasNvidiaDLSSModules)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"DLSSBlueprint",
				"StreamlineBlueprint",
				"StreamlineDLSSGBlueprint",
				"StreamlineReflexBlueprint",
			});
		}

		PublicDefinitions.Add((bHasNvidiaDLSSModules || HasNvidiaDLSSRuntimeDlls())
			? "TIMETHIEF_WITH_NVIDIA_DLSS=1"
			: "TIMETHIEF_WITH_NVIDIA_DLSS=0");

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

	private bool HasNvidiaDLSSRuntimeDlls()
	{
		string ProjectRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", ".."));
		string NvidiaRoot = Path.Combine(ProjectRoot, "Plugins", "NVIDIA");

		return File.Exists(Path.Combine(NvidiaRoot, "DLSS", "Binaries", "ThirdParty", "Win64", "nvngx_dlss.dll"))
			&& File.Exists(Path.Combine(NvidiaRoot, "StreamlineCore", "Binaries", "ThirdParty", "Win64", "nvngx_dlssg.dll"))
			&& File.Exists(Path.Combine(NvidiaRoot, "StreamlineCore", "Binaries", "ThirdParty", "Win64", "sl.common.dll"))
			&& File.Exists(Path.Combine(NvidiaRoot, "StreamlineCore", "Binaries", "ThirdParty", "Win64", "sl.dlss_g.dll"))
			&& File.Exists(Path.Combine(NvidiaRoot, "StreamlineCore", "Binaries", "ThirdParty", "Win64", "sl.interposer.dll"))
			&& File.Exists(Path.Combine(NvidiaRoot, "StreamlineCore", "Binaries", "ThirdParty", "Win64", "sl.reflex.dll"));
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
