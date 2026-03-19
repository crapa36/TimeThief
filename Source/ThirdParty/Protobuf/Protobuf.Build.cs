using UnrealBuildTool;
using System.IO;

public class Protobuf : ModuleRules
{
	public Protobuf(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;
		
		// [UE Project Root]/Extenral/ProtobufBuild/Win64 참조
		string Root = Path.Combine(ModuleDirectory, "..", "..", "..", "External", "ProtobufBuild");
		string Plat = (Target.Platform == UnrealTargetPlatform.Win64) ? "Win64" : Target.Platform.ToString();
		
		string Inc = Path.Combine(Root, Plat, "include");
		string Lib = Path.Combine(Root, Plat, "lib");
		
		// 디버깅용 출력
		// System.Console.WriteLine($"[Protobuf] Inc={Inc}");
		// System.Console.WriteLine($"[Protobuf] Lib={Lib}");
		
		// PublicIncludePaths.Add(Inc);	// 기존 방식 (안통함)
		PublicSystemIncludePaths.Add(Inc);
		
		bEnableExceptions = true;
		bUseRTTI = true;
		
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicAdditionalLibraries.Add(Path.Combine(Lib, "libprotobuf.lib"));
			// 필요하다면 lite 등 다른 버전의 라이브러리도 추가
		}
	}
}