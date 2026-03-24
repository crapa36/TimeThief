

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "TimeThiefNetworkSettings.generated.h"

/**
 * 
 */

class USpawnClassData;
class UTimeThiefPawnData;

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="TimeThief Network Settings"))
class TIMETHIEF_API UTimeThiefNetworkSettings : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Spawn")
	TSoftObjectPtr<USpawnClassData> SpawnClassData;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Spawn")
	TSoftObjectPtr<UTimeThiefPawnData> DefaultLocalPlayerPawnData;
	
};
