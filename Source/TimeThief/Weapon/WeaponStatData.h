#pragma once

#include "CoreMinimal.h"
#include "WeaponStatData.generated.h"

USTRUCT()
struct FWeaponStatData
{
	GENERATED_BODY()
	
	int32 MagCapacity = 0;			// 탄창 용량
	float FireInterval = 0.0f;		// 연사속도 (발사 간격 == 1 / RPM * 60)
	float ReloadTime = 0.0f;		// 재장전 시간
	
	// Shotgun Only
	//-------------------------------------------------------------------------
	int32 PelletCount = 0;			// 발당 총알 수 (샷건 전용)
	float ConeAngle = 0.0f;			// 산탄 범위 각도 (샷건 전용, 단위: 도)
	//-------------------------------------------------------------------------
	
	// Rocket Launcher Only
	//-------------------------------------------------------------------------
	float ProjectileSpeed = 0.0f;	// 발사체 속도 (로켓 런처 전용)
	float ExplosionRadius = 0.0f;	// 폭발 반경 (로켓 런처 전용, 이펙트 영향)
	//-------------------------------------------------------------------------
};
