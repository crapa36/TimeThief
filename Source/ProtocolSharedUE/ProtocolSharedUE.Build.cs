using UnrealBuildTool;
using System.IO;

public class ProtocolSharedUE : ModuleRules
{
	public ProtocolSharedUE(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"Protobuf"
		});

		// ProtocolShared 원본 include 폴더를 직접 include로 노출 (복사 없이)
		string ProjectRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", ".."));
		string PSInclude = Path.Combine(ProjectRoot, "External", "ProtocolShared", "include");
		PublicSystemIncludePaths.Add(PSInclude);
		
		// UE 모듈 내부로 미러링한 pb 헤더/소스 (pb.cc 컴파일용)
		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Private", "Generated"));
		
		ForceIncludeFiles.Add(Path.Combine(ModuleDirectory, "Public", "ProtocolSharedUE_Warnings.h"));
		
		bEnableExceptions = true;
		bUseRTTI = true;
	}
}