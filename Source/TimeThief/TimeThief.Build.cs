// Copyright Epic Games, Inc. All Rights Reserved.

using System;
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

		bool bHasNvidiaDLSSModules = IsPluginAvailable(Target, "DLSS") && IsPluginAvailable(Target, "StreamlineDLSSG");
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

		bool bEnableDLSS = bHasNvidiaDLSSModules || HasNvidiaDLSSRuntimeDlls();
		PublicDefinitions.Add(bEnableDLSS
			? "TIMETHIEF_WITH_NVIDIA_DLSS=1"
			: "TIMETHIEF_WITH_NVIDIA_DLSS=0");

		if (bEnableDLSS && Target.Platform == UnrealTargetPlatform.Win64)
		{
			string ProjectRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", ".."));
			string StreamlineBinariesPath = Path.Combine(ProjectRoot, "Plugins", "Marketplace", "StreamlineCore", "Binaries", "ThirdParty", "Win64");
			if (!Directory.Exists(StreamlineBinariesPath))
			{
				StreamlineBinariesPath = Path.Combine(ProjectRoot, "Plugins", "NVIDIA", "StreamlineCore", "Binaries", "ThirdParty", "Win64");
			}

			string[] StreamlineDlls = new string[]
			{
				"sl.interposer.dll",
				"sl.common.dll",
				"sl.dlss_g.dll",
				"sl.reflex.dll",
				"nvngx_dlssg.dll",
				"nvngx_deepdvc.dll",
				"sl.deepdvc.dll",
				"sl.pcl.dll"
			};

			foreach (string DllName in StreamlineDlls)
			{
				string SourceDllPath = Path.Combine(StreamlineBinariesPath, DllName);
				if (File.Exists(SourceDllPath))
				{
					RuntimeDependencies.Add("$(TargetOutputDir)/" + DllName, SourceDllPath);
				}
			}
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

	private bool IsPluginAvailable(ReadOnlyTargetRules Target, string PluginName)
	{
		if (Target.ProjectFile != null)
		{
			try
			{
				ProjectDescriptor Project = ProjectDescriptor.FromFile(Target.ProjectFile);
				if (Project != null && Project.Plugins != null)
				{
					bool bFound = false;
					bool bEnabled = false;
					foreach (PluginReferenceDescriptor PluginRef in Project.Plugins)
					{
						if (string.Equals(PluginRef.Name, PluginName, StringComparison.OrdinalIgnoreCase))
						{
							bFound = true;
							bEnabled = PluginRef.bEnabled 
								&& PluginRef.IsEnabledForPlatform(Target.Platform) 
								&& PluginRef.IsEnabledForTargetConfiguration(Target.Configuration) 
								&& PluginRef.IsEnabledForTarget(Target.Type);
							break;
						}
					}
					if (bFound && !bEnabled)
					{
						return false;
					}
				}
			}
			catch
			{
				// Ignore errors reading uproject descriptor
			}
		}

		string ProjectRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", ".."));
		string PluginFileName = PluginName + ".uplugin";

		return ContainsPlugin(Path.Combine(ProjectRoot, "Plugins"), PluginFileName)
			|| ContainsPlugin(Path.Combine(EngineDirectory, "Plugins"), PluginFileName);
	}

	private bool HasNvidiaDLSSRuntimeDlls()
	{
		string ProjectRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", ".."));
		string NvidiaRoot = Path.Combine(ProjectRoot, "Plugins", "Marketplace");
		if (!Directory.Exists(Path.Combine(NvidiaRoot, "DLSS")))
		{
			NvidiaRoot = Path.Combine(ProjectRoot, "Plugins", "NVIDIA");
		}

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
