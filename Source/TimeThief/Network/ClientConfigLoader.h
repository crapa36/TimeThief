#pragma once

#include "CoreMinimal.h"
#include "ClientConfigTypes.h"

class FClientConfigLoader
{
public:
	static bool LoadClientConfigFromFile(const FString& FilePath, FClientConfig& OutConfig);
	
private:
	static bool ParseClientConfig(const FString& JsonText, FClientConfig& OutConfig);
	
};
