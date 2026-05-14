#pragma once

#include "CoreMinimal.h"

namespace TimeThiefSmokeParameterDefaults
{
	//박스/렌더 범위

	//연막 실제 충돌/시뮬레이션 내부 박스 반경(cm).
	inline FVector GetSmokeBoundsExtent()
	{
		return FVector(800.0, 800.0, 500.0);
	}

	//내부 박스 외부 추가 렌더링 여백(cm).
	inline FVector GetRenderBoundsPadding()
	{
		return FVector(200.0, 200.0, 200.0);
	}

	//수명/기본 밀도

	//연막 유지 시간(초).
	constexpr float SmokeDuration = 12.0f;

	//유지 후 밀도 페이드아웃 시간(초).
	constexpr float SmokeFadeOutDuration = 5.0f;

	//초기 연막 밀도 배율. 높을수록 더 짙게 시작.
	constexpr float InitialDensity = 0.725f;

	//생성 플룸

	//생성 직후 상승/팽창 플룸 주입 시간(초).
	constexpr float PlumeEmissionDuration = 2.8f;

	//플룸 중심 소스 반경(cm).
	constexpr float PlumeSourceRadius = 75.0f;

	//초기 연막 외향 확산 속도(cm/s).
	constexpr float PlumeExpansionVelocity = 260.0f;

	//초기 연막 상승 속도(cm/s).
	constexpr float PlumeRiseVelocity = 95.0f;

	//장애물 마스크

	//정적 월드 지오메트리 장애물 마스크 사용 여부.
	constexpr bool bUseStaticObstacleMask = true;

	//장애물 마스크 3D 텍스처 한 축 해상도.
	constexpr int32 ObstacleMaskResolution = 32;

	//장애물 마스크 판정 확장 거리(cm).
	constexpr float ObstacleMaskInflation = 6.0f;

	//활성 셀 범위

	//내부 박스 셀 단위 활성 범위 제한 사용 여부.
	constexpr bool bUseBoundsCellCluster = true;

	//내부 박스 활성/비활성 셀 개수(X/Y/Z).
	inline FIntVector GetBoundsCellGrid()
	{
		return FIntVector(6, 6, 4);
	}

	//동시 활성 내부 박스 셀 최대 개수.
	constexpr int32 MaxActiveBoundsCells = 42;

	//폭발 충격에 따른 내부 박스 활성 영역 이동 배율.
	constexpr float ExplosionBoundsShiftScale = 0.4f;

	//GPU 격자/렌더링

	//GPU 연막 시뮬레이션 3D 격자 한 축 해상도.
	constexpr int32 SmokeGridResolution = 64;

	//압력 투영 Jacobi 반복 횟수. 높을수록 속도장 압축 감소.
	constexpr int32 PressureIterations = 10;

	//볼륨 레이마칭 샘플 단계 수. 높을수록 품질/비용 증가.
	constexpr int32 RenderStepCount = 56;

	//연막 내부 빛 흡수/소멸 계수.
	constexpr float Extinction = 2.25f;

	//흡수 대비 산란 비율(0~1). 높을수록 밝게 보임.
	constexpr float ScatteringAlbedo = 0.9f;

	//전방 산란 방향성(-1~1). 양수일수록 빛 방향으로 산란이 강함.
	constexpr float ScatteringAnisotropy = 0.35f;

	//밀도/속도 시뮬레이션

	//초당 밀도 자연 감소 비율.
	constexpr float DensityDissipation = 0.014f;

	//초당 속도장 감쇠 비율.
	constexpr float VelocityDamping = 0.16f;

	//MacCormack 보정 이류 사용 여부. 켜면 보존력/비용 증가.
	constexpr bool bUseMacCormackAdvection = false;

	//와류/난류

	//기본 와류가 밀도/속도장에 반영되는 강도.
	constexpr float VorticityStrength = 0.65f;

	//작은 소용돌이 보존/강조 강도.
	constexpr float VorticityConfinementStrength = 2.25f;

	//공기 상호작용 난류 강도.
	constexpr float TurbulenceStrength = 0.55f;

	//주변 공기 흐름이 속도장에 주는 강도.
	constexpr float AirInteractionStrength = 0.85f;

	//액터/폭발 이벤트 주변 국소 와류 강도.
	constexpr float EventVortexStrength = 1.15f;

	//Warp 꼬리

	//액터 통과 warp 꼬리 초기 강도.
	constexpr float WarpTrailIntensity = 1.1f;

	//warp 초당 감쇠율. 높을수록 빠르게 사라짐.
	constexpr float WarpTrailDecayRate = 8.5f;

	//액터 이벤트 반경 대비 warp 꼬리 두께 배율.
	constexpr float WarpTrailRadiusScale = 0.11f;

	//액터 속도/반경 대비 warp 꼬리 길이 배율.
	constexpr float WarpTrailLengthScale = 7.0f;

	//GPU 이벤트 제한

	//연막당 프레임별 GPU 상호작용 이벤트 최대 개수.
	constexpr int32 MaxGPUEventsPerSmokePerFrame = 96;

	//Carrier particle

	//내부 흐름 보조 carrier particle 개수.
	constexpr int32 CarrierParticleCount = 40;

	//carrier particle 영향 반경(cm).
	constexpr float CarrierParticleRadius = 92.0f;

	//carrier particle 자체 유영 기준 속도(cm/s).
	constexpr float CarrierParticleDriftSpeed = 55.0f;

	//carrier particle 밀도/속도장 영향 배율.
	constexpr float CarrierParticleInteractionStrength = 1.0f;

	//총알 wake

	//총알 통과 시 연막 제거 반경(cm).
	constexpr float BulletClearRadius = 34.0f;

	//총알 궤적 wake 이벤트 샘플 간격(cm).
	constexpr float BulletWakeSampleSpacing = 85.0f;

	//총알 wake 렌더링 유지 최대 시간(초).
	constexpr float BulletWakeMaxVisibleLife = 2.5f;

	//폭발 충격

	//폭발 충격 영향 반경(cm).
	constexpr float ExplosionShockRadius = 420.0f;

	//폭발 충격 속도장 유지 시간(초).
	constexpr float ExplosionImpulseDuration = 0.35f;

	//폭발 외향 밀림 속도 강도(cm/s).
	constexpr float ExplosionOutwardStrength = 900.0f;

	//폭발 즉시 밀도 제거 비율(0~1).
	constexpr float ExplosionDensityClearStrength = 0.25f;

	//액터 상호작용

	//액터-연막 충돌/통과 이벤트 초당 샘플링 빈도.
	constexpr float ActorInteractionHz = 15.0f;

	//액터 밀림 이벤트 인정 최소 속도(cm/s).
	constexpr float ActorPushVelocityThreshold = 80.0f;

	//틱당 액터 상호작용 이벤트 최대 생성 개수.
	constexpr int32 MaxActorInteractionEventsPerTick = 12;

	//디버그

	//연막 내부/렌더 박스 디버그 표시 여부.
	constexpr bool bDrawDebugBounds = false;
}
