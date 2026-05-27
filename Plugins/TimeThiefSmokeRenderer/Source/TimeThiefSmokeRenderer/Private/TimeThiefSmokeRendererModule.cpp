#include "Modules/ModuleManager.h"

#include "Interfaces/IPluginManager.h"
#include "ShaderCore.h"

class FTimeThiefSmokeRendererModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("TimeThiefSmokeRenderer"));
		if (Plugin.IsValid())
		{
			const FString ShaderDir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders"));
			AddShaderSourceDirectoryMapping(TEXT("/TimeThiefSmokeShaders"), ShaderDir);
		}
	}
};

IMPLEMENT_MODULE(FTimeThiefSmokeRendererModule, TimeThiefSmokeRenderer)
