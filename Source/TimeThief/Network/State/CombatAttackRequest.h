#pragma once

#include "CoreMinimal.h"
#include "CombatNotifyType.h"

#include "CombatAttackRequest.generated.h"

USTRUCT(BlueprintType)
struct FCombatAttackRequest
{
	GENERATED_BODY()
	
	ECombatNotifyType NotifyType = ECombatNotifyType::None;
	
	UPROPERTY()
	uint32 AttackId = 0;
	
	UPROPERTY()
	uint32 WeaponId = 0;
	
	UPROPERTY()
	FVector Origin = FVector::ZeroVector;
	
	UPROPERTY()
	FVector Direction = FVector::ForwardVector;
	
	UPROPERTY()
	uint32 TargetEntityId = 0;
	
};