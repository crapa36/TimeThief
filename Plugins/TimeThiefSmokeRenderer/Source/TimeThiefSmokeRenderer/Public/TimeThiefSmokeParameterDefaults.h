#pragma once

#include "CoreMinimal.h"

namespace TimeThiefSmokeParameterDefaults
{
	//박스/렌더 범위

	//연막 실제 충돌/시뮬레이션 내부 박스 반경(센티미터).
	inline FVector GetSmokeBoundsExtent()
	{
		return FVector(500.0, 500.0, 500.0);
	}

	//내부 박스 외부 추가 렌더링 여백(센티미터).
	inline FVector GetRenderBoundsPadding()
	{
		return FVector(300.0, 300.0, 300.0);
	}

	//수명/기본 밀도

	//연막 유지 시간(초).
	constexpr float SmokeDuration = 30.0f;
	//유지 후 밀도 페이드아웃 시간(초).
	constexpr float SmokeFadeOutDuration = 5.0f;
	//초기 연막 밀도 배율. 높을수록 더 짙게 시작.
	constexpr float InitialDensity = 0.8f;

	//생성 플룸

	//생성 직후 상승/팽창 플룸 주입 시간(초).
	constexpr float PlumeEmissionDuration = 2.5f;
	//플룸 중심 소스 반경(센티미터).
	constexpr float PlumeSourceRadius = 100.0f;
	//초기 연막 외향 확산 속도(센티미터/초).
	constexpr float PlumeExpansionVelocity = 200.0f;
	//초기 연막 상승 속도(센티미터/초).
	constexpr float PlumeRiseVelocity = 100.0f;

	//장애물 마스크

	//정적 월드 지오메트리 장애물 마스크 사용 여부.
	constexpr bool bUseStaticObstacleMask = true;
	//장애물 마스크 3차원 텍스처 한 축 해상도.
	constexpr int32 ObstacleMaskResolution = 32;
	//장애물 마스크 판정 확장 거리(센티미터).
	constexpr float ObstacleMaskInflation = 5.0f;

	//활성 셀 범위

	//내부 박스 셀 단위 활성 범위 제한 사용 여부.
	constexpr bool bUseBoundsCellCluster = true;
	//내부 박스 활성/비활성 셀의 각 축 개수.
	inline FIntVector GetBoundsCellGrid()
	{
		return FIntVector(6, 6, 4);
	}
	//동시 활성 내부 박스 셀 최대 개수.
	constexpr int32 MaxActiveBoundsCells = 48;
	//폭발 충격에 따른 내부 박스 활성 영역 이동 배율.
	constexpr float ExplosionBoundsShiftScale = 0.5f;

	//그래픽 격자/렌더링

	//그래픽 연막 시뮬레이션 3차원 격자 한 축 해상도.
	constexpr int32 SmokeGridResolution = 256;
	//압력 투영 자코비 반복 횟수. 높을수록 속도장 압축 감소.
	constexpr int32 PressureIterations = 5;
	//기본 압력 풀이에 멀티그리드를 사용할지 여부. 끄면 훨씬 빠른 자코비 경로를 사용.
	constexpr bool bUseMultigridPressureByDefault = false;
	//볼륨 레이마칭 샘플 단계 수. 높을수록 품질/비용 증가.
	constexpr int32 RenderStepCount = 16;
	//희소 아틀라스 브릭 한 변의 복셀 수.
	constexpr int32 SmokeBrickSize = 16;
	//희소 아틀라스에 저장할 수 있는 활성 연막 브릭 최대 개수.
	constexpr int32 MaxActiveSmokeBricks = 1024;
	//적응형 레이마칭이 사용할 수 있는 최대 샘플 단계 수.
	constexpr int32 RenderMaxStepCount = 126;
	//렌더 레이마칭 목표 단계 길이의 복셀 크기 배율. 높을수록 빠르지만 세부가 줄어듦.
	constexpr float RenderStepVoxelScale = 1.0f;
	//연막 내부 빛 흡수/소멸 계수.
	constexpr float Extinction = 2.0f;
	//흡수 대비 산란 비율(0~1). 높을수록 밝게 보임.
	constexpr float ScatteringAlbedo = 0.9f;
	//전방 산란 방향성(-1~1). 양수일수록 빛 방향으로 산란이 강함.
	constexpr float ScatteringAnisotropy = 0.35f;

	//밀도/속도 시뮬레이션

	//초당 밀도 자연 감소 비율.
	constexpr float DensityDissipation = 0.05f;
	//초당 속도장 감쇠 비율.
	constexpr float VelocityDamping = 0.35f;
	//매코맥 보정 이류 사용 여부. 켜면 보존력/비용 증가.
	constexpr bool bUseMacCormackAdvection = true;
	//비어 있고 영향이 없는 셀에서 매코맥 제한자 계산을 건너뜀.
	constexpr bool bUseAdaptiveMacCormack = true;
	//캐리어 입자 영향을 절반 해상도 3차원 텍스처로 선계산.
	constexpr bool bUseCarrierFieldTexture = true;
	//와류 입자 흩뿌리기를 관련 브릭 주변으로 제한.
	constexpr bool bUseVortexBrickBins = true;

	//와류/난류

	//기본 와류가 밀도/속도장에 반영되는 강도.
	constexpr float VorticityStrength = 0.45f;
	//작은 소용돌이 보존/강조 강도.
	constexpr float VorticityConfinementStrength = 1.5f;
	//공기 상호작용 난류 강도.
	constexpr float TurbulenceStrength = 0.5f;
	//주변 공기 흐름이 속도장에 주는 강도.
	constexpr float AirInteractionStrength = 0.5f;
	//절차적 자체 노이즈 애니메이션 속도 내부 배율.
	constexpr float SelfWobbleTimeScale = 0.05f;
	//자체 컬과 캐리어 표류 속도 내부 배율.
	constexpr float SelfWobbleVelocityScale = 0.25f;
	//이벤트 외 와류, 난류, 주변 공기 힘 내부 배율.
	constexpr float SelfWobbleForceScale = 0.25f;
	//와류 입자 컬 반응과 생성 시 움직임 내부 배율.
	constexpr float SelfWobbleParticleScale = 0.25f;
	//액터/폭발 이벤트 주변 국소 와류 강도.
	constexpr float EventVortexStrength = 0.5f;
	//와류 입자 개수.
	constexpr int32 VortexParticleCount = 48;
	//와류 입자 수명(초).
	constexpr float VortexParticleLifeSeconds = 2.0f;
	//와류 입자 속도장 주입 강도.
	constexpr float VortexParticleStrength = 70.0f;
	//와류 입자 영향 반경(센티미터).
	constexpr float VortexParticleSplatRadius = 150.0f;
	//와류 입자 중심 코어 반경(센티미터).
	constexpr float VortexParticleCoreRadius = 32.0f;
	//밀도 경계에서 와류 입자를 만들 민감도.
	constexpr float VortexDensityGradientScale = 4.0f;

	//워프 꼬리

	//액터 통과 워프 꼬리 초기 강도.
	constexpr float WarpTrailIntensity = 1.5f;
	//워프 초당 감쇠율. 높을수록 빠르게 사라짐.
	constexpr float WarpTrailDecayRate = 1.0f;
	//액터 이벤트 반경 대비 워프 꼬리 두께 배율.
	constexpr float WarpTrailRadiusScale = 0.05f;
	//액터 속도/반경 대비 워프 꼬리 길이 배율.
	constexpr float WarpTrailLengthScale = 8.5f;
	//액터 워프 밀도 누적 배율.
	constexpr float ActorWarpDensityAccumulationScale = 1.5f;
	//액터 워프 누적값 감쇠 시간(초).
	constexpr float ActorWarpAccumulationDecaySeconds = 0.4f;
	//액터 워프 생성 후 남기는 누적 비율.
	constexpr float ActorWarpEmissionRemainder = 0.2f;

	//그래픽 이벤트 제한

	//연막당 프레임별 그래픽 상호작용 이벤트 최대 개수.
	constexpr int32 MaxGPUEventsPerSmokePerFrame = 64;

	//캐리어 입자

	//내부 흐름 보조 캐리어 입자 개수.
	constexpr int32 CarrierParticleCount = 24;
	//캐리어 입자 영향 반경(센티미터).
	constexpr float CarrierParticleRadius = 80.0f;
	//캐리어 입자 자체 유영 기준 속도(센티미터/초).
	constexpr float CarrierParticleDriftSpeed = 16.0f;
	//캐리어 입자 밀도/속도장 영향 배율.
	constexpr float CarrierParticleInteractionStrength = 1.0f;

	//총알 웨이크

	//총알 통과 시 연막 제거 반경(센티미터).
	constexpr float BulletClearRadius = 10.0f;
	//총알 궤적 웨이크 이벤트 샘플 간격(센티미터).
	constexpr float BulletWakeSampleSpacing = 40.0f;
	//총알 웨이크 렌더링 유지 최대 시간(초).
	constexpr float BulletWakeMaxVisibleLife = 0.1f;
	//총알 웨이크 구멍 해제 페이드 시간(초).
	constexpr float BulletWakeReleaseDuration = 2.0f;
	//총알 웨이크 주변 재유입 억제 시간(초).
	constexpr float BulletWakeSinkLife = 0.5f;
	//총알 웨이크 주변 재유입 억제 강도.
	constexpr float BulletWakeSinkStrength = 0.5f;
	//총알 웨이크 진행 방향 속도장 충격 강도.
	constexpr float BulletWakeImpulseStrength = 48.0f;
	//총알 웨이크 가장자리 완화 폭.
	constexpr float BulletWakeCutoutFeather = 1.95f;

	//폭발 충격

	//폭발 충격 영향 반경(센티미터).
	constexpr float ExplosionShockRadius = 500.0f;
	//폭발 충격 속도장 유지 시간(초).
	constexpr float ExplosionImpulseDuration = 0.5f;
	//폭발 외향 밀림 속도 강도(센티미터/초).
	constexpr float ExplosionOutwardStrength = 900.0f;
	//폭발 즉시 밀도 제거 비율(0~1).
	constexpr float ExplosionDensityClearStrength = 0.25f;

	//액터 상호작용

	//액터-연막 충돌/통과 이벤트 초당 샘플링 빈도.
	constexpr float ActorInteractionHz = 10.0f;
	//액터 밀림 이벤트 인정 최소 속도(센티미터/초).
	constexpr float ActorPushVelocityThreshold = 80.0f;
	//틱당 액터 상호작용 이벤트 최대 생성 개수.
	constexpr int32 MaxActorInteractionEventsPerTick = 6;
	//렌더 밀도 노이즈 공간 스케일.
	constexpr float RenderNoiseScale = 0.05f;
	//렌더 밀도 노이즈 강도.
	constexpr float RenderNoiseStrength = 0.5f;
	//렌더 밀도 노이즈 시간 스케일.
	constexpr float RenderNoiseTimeScale = 0.01f;
	//렌더 필라먼트 공간 스케일.
	constexpr float RenderFilamentScale = 0.1;
	//렌더 필라먼트 강도.
	constexpr float RenderFilamentStrength = 1.5f;
	//렌더 필라먼트 대비.
	constexpr float RenderFilamentContrast = 4.5f;
	//렌더 필라먼트 왜곡 강도.
	constexpr float RenderFilamentWarpStrength = 3.75f;

	//디버그

	//연막 내부/렌더 박스 디버그 표시 여부.
	constexpr bool bDrawDebugBounds = false;
}
