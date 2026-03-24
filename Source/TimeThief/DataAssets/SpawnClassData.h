

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Protocol.pb.h"
#include "SpawnClassData.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class TIMETHIEF_API USpawnClassData : public UDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly)
	// TODO: ObjectType 만으로 분기할 수 있나..?
	TMap<int32, TSubclassOf<AActor>> SpawnClassMap;
};
