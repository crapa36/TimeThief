#pragma once

#include "CoreMinimal.h"
#include "CombatNotifyType.h"

#include "RemoteAttackNotify.generated.h"

USTRUCT(BlueprintType)
struct FRemoteAttackNotify
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadWrite)
	ECombatNotifyType NotifyType = ECombatNotifyType::None;
	
	UPROPERTY(BlueprintReadWrite)
	int32 AttackerEntityId = 0;
	
	UPROPERTY(BlueprintReadWrite)
	int32 WeaponId = 0;
	
	UPROPERTY(BlueprintReadWrite)
	int32 ShotSeed = 0;
	
	UPROPERTY(BlueprintReadWrite)
	int32 AttackId = 0;
	
	UPROPERTY(BlueprintReadWrite)
	FVector Origin = FVector::ZeroVector;
	
	UPROPERTY(BlueprintReadWrite)
	FVector Direction = FVector::ForwardVector;
	
	UPROPERTY(BlueprintReadWrite)
	float Range = 0.0f;
	
	UPROPERTY(BlueprintReadWrite)
	int32 SpawnEntityId = 0;
};