#pragma once

#include "CoreMinimal.h"

namespace TimeThiefSmokeParameterDefaults
{
	// 박스/렌더 범위

	// 실제 시뮬레이션/충돌 박스 반경(cm).
	inline FVector GetSmokeBoundsExtent()
	{
		return FVector(500.0, 500.0, 500.0);
	}

	// 시뮬레이션 박스 밖 렌더 여백(cm).
	inline FVector GetRenderBoundsPadding()
	{
		return FVector(300.0, 300.0, 300.0);
	}

	// 수명/기본 밀도

	// 연막 유지 시간(초).
	constexpr float SmokeDuration = 30.0f;
	// 수명 끝 페이드아웃 시간(초).
	constexpr float SmokeFadeOutDuration = 5.0f;
	// 초기 연막 밀도 배율.
	constexpr float InitialDensity = 0.8f;

	// 생성 플룸

	// 생성 직후 상승/확산 플룸 주입 시간(초).
	constexpr float PlumeEmissionDuration = 2.5f;
	// 플룸 중심 소스 반경(cm).
	constexpr float PlumeSourceRadius = 100.0f;
	// 초기 연막 수평 확산 속도(cm/s).
	constexpr float PlumeExpansionVelocity = 200.0f;
	// 초기 연막 상승 속도(cm/s).
	constexpr float PlumeRiseVelocity = 100.0f;

	// 장애물 마스크

	// 정적 필드 지오메트리 장애물 마스크 사용 여부. 켜면 충돌 품질과 비용 증가.
	constexpr bool bUseStaticObstacleMask = true;
	// 장애물 마스크 3D 텍스처 축 해상도. 높을수록 충돌 품질과 비용 증가.
	constexpr int32 ObstacleMaskResolution = 24;
	// 장애물 마스크 보정 팽창 거리(cm). 높을수록 장애물 영향 범위 증가.
	constexpr float ObstacleMaskInflation = 5.0f;

	// 활성 셀 범위

	// 시뮬레이션 박스 내부 단위 활성 범위 제한 사용 여부. 켜면 비용 감소, 품질 제한 가능.
	constexpr bool bUseBoundsCellCluster = true;
	// 내부 박스 활성/비활성 관리 격자 수. 높을수록 제어 정밀도와 비용 증가.
	inline FIntVector GetBoundsCellGrid()
	{
		return FIntVector(6, 6, 4);
	}
	// 동시에 활성화되는 내부 박스 셀 최대 개수. 높을수록 품질과 비용 증가.
	constexpr int32 MaxActiveBoundsCells = 48;
	// 폭발 추적에 따른 내부 박스 활성 영역 이동 배율. 높을수록 반응 범위 증가.
	constexpr float ExplosionBoundsShiftScale = 0.5f;

	// 그리드/렌더마치

	// 연막 시뮬레이션 3D 격자 축 해상도. 높을수록 품질과 비용 크게 증가.
	constexpr int32 SmokeGridResolution = 256;
	// 압력 투영 Jacobi 반복 횟수. 높을수록 압력 품질과 비용 증가.
	constexpr int32 PressureIterations = 4;
	// 기본 압력 단계에서 멀티그리드 사용 여부. 켜면 품질/성능 균형 개선 가능.
	constexpr bool bUseMultigridPressureByDefault = false;
	// 기본 렌더 raymarch 샘플 단계 수. 높을수록 렌더 품질과 비용 증가.
	constexpr int32 RenderStepCount = 16;
	// sparse 인덱스 브릭 한 변 크기. 높을수록 관리 비용 감소, 세밀도 감소 가능.
	constexpr int32 SmokeBrickSize = 16;
	// sparse가 보관할 활성 연막 브릭 최대 개수. 높을수록 품질과 VRAM 비용 증가.
	constexpr int32 MaxActiveSmokeBricks = 2048;
	// 적응형 raymarch가 사용할 수 있는 최대 샘플 단계 수. 높을수록 품질과 비용 증가.
	constexpr int32 RenderMaxStepCount = 128;
	// 렌더 raymarch 목표 스텝 길이의 보셀 크기 배율. 높을수록 비용 감소, 품질 감소.
	constexpr float RenderStepVoxelScale = 1.25f;
	// 연막 내부 빛 흡수/소멸 계수. 높을수록 더 어둡고 짙게 보임.
	constexpr float Extinction = 2.15f;
	// 흡수 대비 산란 비율(0~1). 높을수록 밝게 보임.
	constexpr float ScatteringAlbedo = 0.9f;
	// 전방 산란 방향성(-1~1). 높을수록 빛 방향 산란이 강해짐.
	constexpr float ScatteringAnisotropy = 0.35f;

	// 밀도/속도 시뮬레이션

	// 초당 밀도 자연 감소 비율. 높을수록 연막이 빨리 옅어짐.
	constexpr float DensityDissipation = 0.05f;
	// 초당 속도 감쇠 비율. 높을수록 움직임이 빨리 안정됨.
	constexpr float VelocityDamping = 0.4f;
	// MacCormack 보정 이류 사용 여부. 켜면 이류 품질과 비용 증가.
	constexpr bool bUseMacCormackAdvection = true;
	// 영향 없는 빈 셀에서 MacCormack 제한 계산 생략 여부. 켜면 비용 감소, 품질 유지.
	constexpr bool bUseAdaptiveMacCormack = true;
	// 캐리어 입자 영향을 half-res 3D 텍스처로 선계산할지 여부. 켜면 반복 비용 감소.
	constexpr bool bUseCarrierFieldTexture = true;
	// 와류 입자 splat을 브릭 주변으로 제한할지 여부. 켜면 다중 연막 비용 감소.
	constexpr bool bUseVortexBrickBins = true;

	// 와류/흔들림

	// 기본 와류가 밀도/속도장에 반영되는 강도. 높을수록 움직임과 비용 증가.
	constexpr float VorticityStrength = 0.45f;
	// 작은 소용돌이 보존/강조 강도. 높을수록 디테일과 비용 증가.
	constexpr float VorticityConfinementStrength = 1.5f;
	// 공기 난류 강도. 높을수록 흔들림과 비용 증가.
	constexpr float TurbulenceStrength = 0.5f;
	// 주변 공기 흐름이 속도장에 주는 강도. 높을수록 자연 흐름과 비용 증가.
	constexpr float AirInteractionStrength = 0.5f;
	// 자체 노이즈 애니메이션 속도 배율. 높을수록 더 빠르게 꿀렁임.
	constexpr float SelfWobbleTimeScale = 0.05f;
	// 자체 컬/캐리어 와류 속도 배율. 높을수록 움직임과 비용 증가.
	constexpr float SelfWobbleVelocityScale = 0.25f;
	// 이벤트 외 와류, 주변 공기 힘 배율. 높을수록 움직임과 비용 증가.
	constexpr float SelfWobbleForceScale = 0.25f;
	// 와류 입자 컬 반응과 생성 후 이동 배율. 높을수록 움직임과 비용 증가.
	constexpr float SelfWobbleParticleScale = 0.25f;
	// 총알/폭발/액터 이벤트 주변 국소 와류 강도. 높을수록 반응과 비용 증가.
	constexpr float EventVortexStrength = 0.5f;
	// 와류 입자 개수. 높을수록 디테일과 비용 증가.
	constexpr int32 VortexParticleCount = 48;
	// 와류 입자 수명(초). 높을수록 디테일 지속과 비용 증가.
	constexpr float VortexParticleLifeSeconds = 2.0f;
	// 와류 입자 속도장 주입 강도. 높을수록 움직임과 비용 증가.
	constexpr float VortexParticleStrength = 70.0f;
	// 와류 입자 영향 반경(cm). 높을수록 영향 범위와 비용 증가.
	constexpr float VortexParticleSplatRadius = 150.0f;
	// 와류 입자 중심 코어 반경(cm). 높을수록 중심 영향 범위 증가.
	constexpr float VortexParticleCoreRadius = 32.0f;
	// 밀도 경계에서 와류 입자를 만드는 민감도. 높을수록 디테일과 비용 증가.
	constexpr float VortexDensityGradientScale = 4.0f;

	// 워프 꼬리

	// 액터 통과 워프 꼬리 초기 강도. 높을수록 시각 디테일과 비용 증가.
	constexpr float WarpTrailIntensity = 1.5f;
	// 워프 초당 감쇠율. 높을수록 더 빨리 사라져 비용 감소.
	constexpr float WarpTrailDecayRate = 1.0f;
	// 액터 반경 대비 워프 꼬리 두께 배율. 높을수록 범위와 비용 증가.
	constexpr float WarpTrailRadiusScale = 0.05f;
	// 액터 속도/반경 대비 워프 꼬리 길이 배율. 높을수록 범위와 비용 증가.
	constexpr float WarpTrailLengthScale = 7.0f;
	// 액터 워프 밀도 누적 배율. 높을수록 흔적 지속과 비용 증가.
	constexpr float ActorWarpDensityAccumulationScale = 1.5f;
	// 액터 워프 누적값 감쇠 시간(초). 높을수록 흔적 지속과 비용 증가.
	constexpr float ActorWarpAccumulationDecaySeconds = 0.3f;
	// 액터 워프 생성 후 남기는 누적 비율. 높을수록 잔상과 비용 증가.
	constexpr float ActorWarpEmissionRemainder = 0.2f;
	// 액터가 지나갈 때 공기 흐름을 속도장에 반영하는 전체 강도. 높을수록 반응과 비용 증가.
	constexpr float ActorAirflowStrength = 1.0f;
	// 액터 공기 흐름이 시작되는 최소 속도(cm/s). 높을수록 반응 빈도와 비용 감소.
	constexpr float ActorAirflowMinSpeed = 24.0f;
	// 액터 공기 흐름이 최대 반응에 도달하는 속도(cm/s). 높을수록 강한 반응이 늦어짐.
	constexpr float ActorAirflowFullSpeed = 320.0f;
	// 액터 크기 대비 공기 흐름 영향 반경 배율. 높을수록 범위와 비용 증가.
	constexpr float ActorAirflowRadiusScale = 3.8f;
	// 액터 전방 압축 공기 흐름 강도. 높을수록 앞쪽 반응과 비용 증가.
	constexpr float ActorAirflowFrontStrength = 0.42f;
	// 액터 측면으로 빠져나가는 공기 흐름 강도. 높을수록 측면 반응과 비용 증가.
	constexpr float ActorAirflowSideStrength = 0.26f;
	// 액터 후방으로 끌려가는 공기 흐름 강도. 높을수록 뒤쪽 흔적과 비용 증가.
	constexpr float ActorAirflowWakeStrength = 0.34f;
	// 액터 이동 공기 흐름이 추가로 만드는 와류 강도. 높을수록 회전 반응과 비용 증가.
	constexpr float ActorAirflowVortexStrength = 0.18f;

	// 그래픽 이벤트 제한

	// 연막당 프레임별 GPU 상호작용 이벤트 최대 개수. 높을수록 반응 품질과 비용 증가.
	constexpr int32 MaxGPUEventsPerSmokePerFrame = 64;
	// sparse brick 활성화에 쓰는 최소 속도(cm/s). 높을수록 비용 감소, 저속 움직임 감소.
	constexpr float SparseVelocityActiveThreshold = 120.0f;

	// 캐리어 입자

	// 내부 흐름 보조 캐리어 입자 개수. 높을수록 흐름 품질과 비용 증가.
	constexpr int32 CarrierParticleCount = 20;
	// 캐리어 입자 영향 반경(cm). 높을수록 범위와 비용 증가.
	constexpr float CarrierParticleRadius = 90.0f;
	// 캐리어 입자 자체 유영 기준 속도(cm/s). 높을수록 내부 움직임 증가.
	constexpr float CarrierParticleDriftSpeed = 16.0f;
	// 캐리어 입자 밀도/속도 영향 배율. 높을수록 반응과 비용 증가.
	constexpr float CarrierParticleInteractionStrength = 1.0f;

	// 총알 웨이크

	// 총알 통과 시 연막 제거 반경(cm). 높을수록 구멍 크기와 비용 증가.
	constexpr float BulletClearRadius = 40.0f;
	// 총알 웨이크 렌더 최대 가시 시간(초). 높을수록 흔적 지속과 비용 증가.
	constexpr float BulletWakeMaxVisibleLife = 1.0f;
	// 총알 웨이크 구멍 해제 페이드 시간(초). 높을수록 복귀가 느리고 비용 증가.
	constexpr float BulletWakeReleaseDuration = 2.0f;
	// 총알 웨이크 주변 자유 공간 억제 시간(초). 높을수록 구멍 유지와 비용 증가.
	constexpr float BulletWakeSinkLife = 0.5f;
	// 총알 웨이크 주변 자유 공간 억제 강도. 높을수록 구멍 선명도와 비용 증가.
	constexpr float BulletWakeSinkStrength = 0.5f;
	// 총알 진행 방향 속도장 충격 강도. 높을수록 반응과 비용 증가.
	constexpr float BulletWakeImpulseStrength = 48.0f;
	// 총알 웨이크 가장자리 완화 폭. 높을수록 부드럽고 선명도 감소.
	constexpr float BulletWakeCutoutFeather = 1.95f;

	// 폭발 충격

	// 폭발 충격 영향 반경(cm). 높을수록 범위와 비용 증가.
	constexpr float ExplosionShockRadius = 500.0f;
	// 폭발 충격 속도장 유지 시간(초). 높을수록 반응 지속과 비용 증가.
	constexpr float ExplosionImpulseDuration = 0.5f;
	// 폭발 바깥 방향 속도 강도(cm/s). 높을수록 반응과 비용 증가.
	constexpr float ExplosionOutwardStrength = 720.0f;
	// 폭발 즉시 밀도 제거 비율(0~1). 높을수록 제거 강도 증가.
	constexpr float ExplosionDensityClearStrength = 0.25f;

	// 액터 상호작용

	// 액터-연막 충돌/통과 이벤트 초당 샘플 빈도. 높을수록 반응 품질과 CPU/GPU 비용 증가.
	constexpr float ActorInteractionHz = 10.0f;
	// 액터 밀림 이벤트 인정 최소 속도(cm/s). 높을수록 비용 감소, 약한 반응 감소.
	constexpr float ActorPushVelocityThreshold = 10.0f;
	// 틱당 액터 상호작용 이벤트 최대 생성 개수. 높을수록 반응 품질과 비용 증가.
	constexpr int32 MaxActorInteractionEventsPerTick = 32;
	// 렌더 밀도 노이즈 공간 스케일. 높을수록 패턴이 촘촘하고 비용 유사.
	constexpr float RenderNoiseScale = 0.05f;
	// 렌더 밀도 노이즈 강도. 높을수록 디테일과 비용 증가.
	constexpr float RenderNoiseStrength = 0.5f;
	// 렌더 밀도 노이즈 시간 스케일. 높을수록 노이즈 움직임 증가.
	constexpr float RenderNoiseTimeScale = 0.01f;
	// 렌더 필라먼트 공간 스케일. 높을수록 패턴이 촘촘하고 비용 유사.
	constexpr float RenderFilamentScale = 0.1f;
	// 렌더 필라먼트 강도. 높을수록 디테일과 비용 증가.
	constexpr float RenderFilamentStrength = 1.2f;
	// 렌더 필라먼트 대비. 높을수록 선명도와 aliasing 위험 증가.
	constexpr float RenderFilamentContrast = 4.0f;
	// 렌더 필라먼트 휘어짐 강도. 높을수록 디테일과 비용 증가.
	constexpr float RenderFilamentWarpStrength = 2.8f;

	// 디버그

	// 연막 내부/렌더 박스 디버그 표시 여부. 켜면 디버그 가시성과 비용 증가.
	constexpr bool bDrawDebugBounds = false;
}
