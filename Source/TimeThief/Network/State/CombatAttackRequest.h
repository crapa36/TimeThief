#pragma once

#include "CoreMinimal.h"
#include "CombatNotifyType.h"

#include "CombatAttackRequest.generated.h"

USTRUCT(BlueprintType)
struct FCombatAttackRequest
{
	GENERATED_BODY()
	
	// 어떤 종류의 행위인지
	ECombatNotifyType NotifyType = ECombatNotifyType::None;
	
	UPROPERTY()
	// NPC가 어떤 공격을 하는지
	uint32 AttackId = 0;
	
	UPROPERTY()
	// 플레이어가 어떤 무기를 사용하는지
	uint32 WeaponId = 0;
	
	UPROPERTY()
	// 무기가 샷건인 경우 필요한 샷 시드값 (샷건의 탄퍼짐 계산에 사용)
	uint32 ShotSeed = 0;
	
	UPROPERTY()
	// 공격의 시작 위치
	FVector Origin = FVector::ZeroVector;
	
	UPROPERTY()
	// 공격의 방향 (정규화된 벡터)
	FVector Direction = FVector::ForwardVector;
	
	UPROPERTY()
	// 만약 Target이 있다면 어떤 대상인지
	uint32 TargetEntityId = 0;
	
};