

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Protocol.pb.h"
#include "SpawnClassData.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class TIMETHIEF_API USpawnClassData : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly)
	// TODO: ObjectType 만으로 분기할 수 있나..?
	TMap<int32, TSubclassOf<AActor>> SpawnClassMap;

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<AActor> LocalPlayerClass;

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<AActor> RemotePlayerClass;
	
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<AActor> RocketProjectileClass;
	
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<AActor> ChestClass;
	
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<AActor> ItemClass;
	
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<AActor> StoreClass;
	
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<AActor> TestMonster;
	
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<AActor> CatMonster;
	
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<AActor> MinionMonster;
	
};
