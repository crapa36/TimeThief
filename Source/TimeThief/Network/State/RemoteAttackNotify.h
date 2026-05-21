#pragma once

#include "CoreMinimal.h"
#include "CombatNotifyType.h"

#include "RemoteAttackNotify.generated.h"

USTRUCT(BlueprintType)
struct FRemoteAttackNotify
{
	GENERATED_BODY()
	
	UPROPERTY()
	ECombatNotifyType NotifyType = ECombatNotifyType::None;
	
	UPROPERTY()
	uint32 AttackerEntityId = 0;
	
	UPROPERTY()
	uint32 WeaponId = 0;
	
	UPROPERTY()
	uint32 ShotSeed = 0;
	
	UPROPERTY()
	uint32 AttackId = 0;
	
	UPROPERTY()
	FVector Origin = FVector::ZeroVector;
	
	UPROPERTY()
	FVector Direction = FVector::ForwardVector;
	
};
