#pragma once

#include "CoreMinimal.h"

namespace TimeThiefSmokeParameterDefaults
{
	// 파라미터 주석은 용도와 단위를 먼저 쓰고, 값 변화가 품질·비용·반응에 주는 영향을 뒤에 적는다.

	// 범위, 수명

	// 실제 시뮬레이션 연막 박스 반경(cm). 높을수록 연막 범위와 시뮬레이션 비용 증가.
	inline FVector GetSmokeBoundsExtent()
	{
		return FVector(700.0, 700.0, 400.0);
	}

	// 렌더링 박스에 더하는 여유 반경(cm). 높을수록 가장자리 잘림 감소, raymarch 범위 증가.
	inline FVector GetRenderBoundsPadding()
	{
		return FVector(100.0, 100.0, 100.0);
	}

	// 연막 지속 시간(초). 높을수록 연막 유지 시간 증가.
	constexpr float SmokeDuration = 30.0f;
	// 연막 페이드아웃 시간(초). 높을수록 소멸이 부드럽고 전체 수명 증가.
	constexpr float SmokeFadeOutDuration = 5.0f;
	// 초기 연막 밀도. 높을수록 생성 직후 불투명도 증가.
	constexpr float InitialDensity = 5.0f;
	// 시뮬레이션 밀도 상한. 높을수록 진한 연막 보존, 과포화 위험 증가.
	constexpr float SmokeDensityMax = 5.0f;

	// 생성 플룸

	// 생성 직후 밀도와 속도 소스를 넣는 시간(초). 높을수록 플룸 성장 시간과 밀도 증가.
	constexpr float PlumeEmissionDuration = 2.5f;
	// 생성 소스 반경(cm). 높을수록 초기 연막 덩어리가 커짐.
	constexpr float PlumeSourceRadius = 100.0f;
	// 초기 연막 수평 확산 속도(cm/s). 높을수록 생성 직후 수평 확산 증가.
	constexpr float PlumeExpansionVelocity = 800.0f;
	// 초기 연막 상승 속도(cm/s). 높을수록 생성 직후 상승 이동 증가.
	constexpr float PlumeRiseVelocity = 30.0f;

	// 장애물 마스크

	// 장애물 마스크 3D 텍스처 축 해상도. 높을수록 충돌 품질과 비용 증가.
	constexpr int32 ObstacleMaskResolution = 16;
	// 장애물 검사 박스 최소 여유 거리(cm). 높을수록 얇은 장애물 검출 증가, 과차단 위험 증가.
	constexpr float ObstacleMaskInflation = 1.0f;
	// 장애물 마스크 복셀 검사 박스의 셀 반경 비율. 높을수록 누락 감소, 과차단 증가.
	constexpr float ObstacleMaskCellFootprintRatio = 0.25f;
	// 생성 지점이 표면에 살짝 묻을 때 여는 반경 배율. 높을수록 spawn 접촉 보정 증가.
	constexpr float ObstacleSourceClearRadiusScale = 1.5f;
	// 한 연막에 업로드할 장애물 프리미티브 최대 개수. 높을수록 복잡한 장애물 보존과 GPU 비용 증가.
	constexpr int32 MaxObstaclePrimitives = 32;
	// 장애물이 없는 영역에 기록할 SDF 거리(cm). 높을수록 유효 표면 거리 범위 증가.
	constexpr float ObstacleFieldFarDistanceCm = 100000.0f;
	// 장애물 SDF 표면 페더 폭(cm). 높을수록 경계가 부드럽고 차단 영역 증가.
	constexpr float ObstacleSdfSurfaceFeatherCm = 32.0f;
	// 장애물 transform 위치 변경 감지 허용오차(cm). 높을수록 갱신 비용과 미세 이동 반응 감소.
	constexpr float ObstacleTransformLocationToleranceCm = 0.1f;
	// 장애물 transform 스케일 변경 감지 허용오차. 높을수록 갱신 비용과 미세 스케일 반응 감소.
	constexpr float ObstacleTransformScaleTolerance = 0.001f;
	// 장애물 transform 회전 변경 감지 허용오차(rad). 높을수록 갱신 비용과 미세 회전 반응 감소.
	constexpr float ObstacleTransformRotationToleranceRadians = 0.001f;

	// 시뮬레이션 격자

	// 연막 3D 격자 기준 축 해상도. 높을수록 시뮬레이션 품질과 비용 증가.
	constexpr int32 SmokeGridResolution = 16;
	// 시뮬레이션 그리드 최소 축 해상도. 낮을수록 비용 감소, 품질 감소.
	constexpr int32 SmokeGridMinAxisResolution = 16;
	// 시뮬레이션 그리드 최대 축 해상도. 높을수록 품질과 비용 증가.
	constexpr int32 SmokeGridMaxAxisResolution = 32;
	// 컴퓨트 셰이더 스레드 그룹 한 변 크기. 셰이더 numthreads와 맞아야 함.
	constexpr int32 SmokeThreadGroupSize = 4;
	// 격자 재할당 축 정렬 단위. 높을수록 재할당 감소, 메모리 여유 증가.
	constexpr int32 SmokeGridAllocationQuantum = 16;
	// Jacobi 압력 반복 횟수. 높을수록 압력 품질과 비용 증가.
	constexpr int32 PressureIterations = 1;
	// 압력 반복 최소 횟수. 낮을수록 비용 감소, 발산 제거 품질 감소.
	constexpr int32 PressureIterationsMin = 1;
	// 압력 반복 최대 횟수. 높을수록 압력 품질과 비용 증가.
	constexpr int32 PressureIterationsMax = 2;
	// 희소 브릭 한 변의 복셀 수. 높을수록 관리 비용 감소, 빈 공간 낭비 증가.
	constexpr int32 SmokeBrickSize = 16;
	// sparse brick 최소 크기(voxel). 낮을수록 culling 정밀도와 비용 증가.
	constexpr int32 SmokeBrickMinSize = 4;
	// sparse brick 최대 크기(voxel). 높을수록 비용 감소, culling 정밀도 감소.
	constexpr int32 SmokeBrickMaxSize = 32;
	// 희소 아틀라스 활성 브릭 최대 개수. 높을수록 넓은 연막 지원과 VRAM 비용 증가.
	constexpr int32 MaxActiveSmokeBricks = 32;
	// 희소 브릭 활성화 최소 속도(cm/s). 높을수록 비용 감소, 약한 움직임 손실 증가.
	constexpr float SparseVelocityActiveThreshold = 150.0f;

	// 렌더링 raymarch

	// 카메라 ray에 고정하는 월드 기준 raymarch 샘플 간격(cm). 높을수록 비용과 공간 디테일 감소.
	constexpr float RenderWorldStepLengthCm = 20.0f;
	// 월드 기준 raymarch 샘플 간격 최소값(cm). 낮을수록 허용 가능한 최고 품질과 비용 증가.
	constexpr float RenderWorldStepLengthMinCm = 10.0f;
	// 월드 기준 raymarch 샘플 간격 최대값(cm). 높을수록 허용 가능한 최저 비용과 디테일 손실 증가.
	constexpr float RenderWorldStepLengthMaxCm = 200.0f;
	// 반해상도 합성 기본 사용 여부. 켜면 raymarch 비용 감소, 업샘플 경계 오차 증가.
	constexpr int32 HalfResolution = 1;
	// 멀티 연막 composite tile 한 변 크기(px). 높을수록 타일 관리 비용 감소, culling 정밀도 감소.
	constexpr int32 CompositeTileSize = 32;
	// 한 번에 합성할 연막 슬롯 최대 개수. 높을수록 겹친 연막 처리량과 비용 증가.
	constexpr int32 MaxCompositeSmokeSlots = 8;
	// fullscreen composite 전환 화면 면적 비율. 낮을수록 fullscreen 경로를 더 빨리 사용.
	constexpr float CompositeFullscreenAreaThreshold = 0.58f;
	// scissor를 적용할 최소 절약 픽셀 수. 높을수록 작은 영역 최적화와 설정 비용 감소.
	constexpr int32 CompositeScissorMinSavedPixels = 200000;
	// 연막 screen rect 여유 픽셀(px). 높을수록 가장자리 누락 감소, 합성 면적 증가.
	constexpr int32 CompositeScreenRectPadding = 8;
	// 빛과 그림자

	// 연막 흡수 계수. 높을수록 더 어둡고 불투명하게 보임.
	constexpr float Extinction = 3.0f;
	// 전방 산란 방향성(-1~1). 높을수록 빛 방향 하이라이트 증가.
	constexpr float ScatteringAnisotropy = 0.2f;
	// 자체 그림자 기준 빛 방향.
	inline FVector3f GetSelfShadowLightDirection()
	{
		return FVector3f(-0.45f, -0.25f, 0.86f).GetSafeNormal();
	}
	// 자체 그림자 적용 강도(0~1). 높을수록 연막 내부 그림자 증가.
	constexpr float SelfShadowStrength = 0.8f;
	// 자체 그림자 흡수 계수. 높을수록 그림자가 진해짐.
	constexpr float SelfShadowExtinction = 1.0f;
	// 결합 밀도장 그림자 샘플 수. 높을수록 그림자 품질과 비용 증가.
	constexpr int32 CombinedShadowStepCount = 32;
	// 결합 밀도장 그림자 샘플 수 최소값. 낮을수록 그림자를 완전히 끌 수 있음.
	constexpr int32 CombinedShadowStepCountMin = 0;
	// 결합 밀도장 그림자 샘플 수 최대값. 높을수록 허용 가능한 그림자 품질과 비용 증가.
	constexpr int32 CombinedShadowStepCountMax = 32;
	// 결합 밀도장 그림자 샘플 간격(cm). 높을수록 넓은 범위를 덮지만 세부 정확도 감소.
	constexpr float CombinedShadowStepLength = 48.0f;
	// 결합 밀도장 그림자 샘플 간격 최소값(cm). 낮을수록 허용 가능한 세부 정확도와 비용 증가.
	constexpr float CombinedShadowStepLengthMinCm = 10.0f;
	// 결합 밀도장 그림자 샘플 간격 최대값(cm). 높을수록 허용 가능한 범위와 세부 손실 증가.
	constexpr float CombinedShadowStepLengthMaxCm = 200.0f;
	// 자체 그림자를 계산할 최소 샘플 기여도. 높을수록 그림자 비용 감소.
	constexpr float SelfShadowMinSampleWeight = 0.02f;
	constexpr float SelfShadowFullRateSampleWeight = 0.04f;
	constexpr int32 SelfShadowLowContributionStride = 2;
	// raymarch 투과율 종료 기준. 높을수록 비용 감소, 진한 연막 누적 손실 증가.
	constexpr float RenderTransmittanceEarlyOut = 0.02f;
	// 장애물 마스크 CPU 캐시 최대 개수. 높을수록 반복 생성 비용 감소, 메모리 증가.
	constexpr int32 ObstacleMaskCacheMaxEntries = 64;

	// 렌더링 디테일

	// 렌더 노이즈 공간 스케일. 높을수록 패턴이 촘촘해짐.
	constexpr float RenderNoiseScale = 0.01f;
	// 렌더 노이즈 강도. 높을수록 밀도 디테일 증가.
	constexpr float RenderNoiseStrength = 0.8f;
	// 렌더 노이즈 시간 스케일. 높을수록 노이즈 움직임 증가.
	constexpr float RenderNoiseTimeScale = 0.035f;
	// 렌더 경계 노이즈 공간 스케일(1/cm). 높을수록 경계 무늬가 촘촘해짐.
	constexpr float RenderBoundaryNoiseScale = 0.01f;
	// 렌더 경계 노이즈 강도. 높을수록 경계가 깨지지만 물결 artifact 위험 증가.
	constexpr float RenderBoundaryNoiseStrength = 0.16f;
	// 필라멘트 공간 스케일. 높을수록 필라멘트가 촘촘해짐.
	constexpr float RenderFilamentScale = 0.01f;
	// 필라멘트 강도. 높을수록 선형 디테일 증가.
	constexpr float RenderFilamentStrength = 1.4f;
	// 필라멘트 대비. 높을수록 선명도 증가, aliasing 위험 증가.
	constexpr float RenderFilamentContrast = 1.0f;
	// 필라멘트 도메인 워프 강도. 높을수록 꼬임 디테일 증가.
	constexpr float RenderFilamentWarpStrength = 1.5f;
	// 렌더 노이즈와 필라멘트 계산을 생략할 밀도 하한. 낮을수록 비용과 고밀도 내부 디테일 감소.
	constexpr float RenderDetailDensityCutoff = 3.5;
	constexpr float RenderDetailMinDensity = 0.01f;
	constexpr float RenderDetailDensityFadeEnd = 0.05f;
	constexpr float RenderDetailFullDistanceCm = 1200.0f;
	constexpr float RenderDetailCullDistanceCm = 3000.0f;
	constexpr float RenderDetailMinScreenFraction = 0.015f;
	constexpr float RenderDetailFullScreenFraction = 0.08f;

	// 이류와 감쇠

	// 초당 밀도 자연 감소율. 높을수록 연막이 빨리 옅어짐.
	constexpr float DensityDissipation = 0.02f;
	// 초당 속도 감쇠율. 높을수록 움직임이 빨리 안정됨.
	constexpr float VelocityDamping = 0.2f;

	// 와류와 흔들림

	// 기본 와류 강도. 높을수록 소용돌이 움직임 증가.
	constexpr float VorticityStrength = 1.0f;
	// 와류 보존 강도. 높을수록 회전 구조 유지와 비용 증가.
	constexpr float VorticityConfinementStrength = 2.0f;
	// 난류 힘 강도. 높을수록 불규칙 움직임 증가.
	constexpr float TurbulenceStrength = 1.0f;
	// 주변 공기 흐름 반응 강도. 높을수록 외부 힘 반응 증가.
	constexpr float AirInteractionStrength = 0.75f;
	// 자체 흔들림 시간 스케일. 높을수록 흔들림 변화 속도 증가.
	constexpr float SelfWobbleTimeScale = 0.05f;
	// 자체 흔들림 속도 스케일. 높을수록 내부 움직임 증가.
	constexpr float SelfWobbleVelocityScale = 0.25f;
	// 이벤트 주변 자체 흔들림 힘 배율. 높을수록 이벤트 반응 증가.
	constexpr float SelfWobbleForceScale = 0.6f;
	// 와류 입자 자체 흔들림 배율. 높을수록 와류 입자 움직임 증가.
	constexpr float SelfWobbleParticleScale = 0.3f;
	// 이벤트가 만드는 국소 와류 강도. 높을수록 총알/폭발/액터 반응 증가.
	constexpr float EventVortexStrength = 1.5f;
	// 와류 입자 수. 높을수록 디테일과 비용 증가.
	constexpr int32 VortexParticleCount = 24;
	// 와류 입자 최대 수. 높을수록 설정 허용 범위와 버퍼 비용 증가.
	constexpr int32 MaxVortexParticleCount = 32;
	// 와류 입자 수명(초). 높을수록 와류 디테일 지속 시간 증가.
	constexpr float VortexParticleLifeSeconds = 2.0f;
	// 와류 입자 최소 수명(초). 높을수록 짧은 와류 보존과 잔류 비용 증가.
	constexpr float VortexParticleMinLifeSeconds = 0.05f;
	// 와류 입자 속도 주입 강도(cm/s). 높을수록 국소 회전 움직임 증가.
	constexpr float VortexParticleStrength = 96.0f;
	// 연막 속도 상한(cm/s). 높을수록 충돌/폭발 반응이 강해짐.
	constexpr float MaxSmokeVelocity = 2600.0f;
	// 와류 입자 영향 반경(cm). 높을수록 영향 범위와 비용 증가.
	constexpr float VortexParticleSplatRadius = 180.0f;
	// 와류 입자 중심 코어 반경(cm). 높을수록 중심 영향 범위 증가.
	constexpr float VortexParticleCoreRadius = 45.0f;
	// 와류 입자 반경 최소값(cm).
	constexpr float VortexParticleMinRadius = 1.0f;
	// 밀도 경계 와류 입자 생성 민감도. 높을수록 경계 와류 생성 증가.
	constexpr float VortexDensityGradientScale = 4.0f;
	// 와류 입자 업데이트 간격(초). 높을수록 비용 감소, 반응 지연 증가.
	constexpr float VortexSubstepIntervalSeconds = 1.0f / 8.0f;
	// 액터 wake와 공기 흐름

	// 액터가 지나간 뒤쪽 wake 길이 배율. 높을수록 물리 영향 범위 증가.
	constexpr float ActorWakeTrailLengthScale = 5.0f;
	// 액터 wake street lane 내부 반경 배율. 낮을수록 얇은 후류 생성.
	constexpr float ActorWakeStreetLaneInnerRadiusScale = 0.28f;
	// 액터 표면 회전 힘(cm/s). 높을수록 몸 표면을 타는 연기 회전 증가.
	constexpr float ActorWakeSurfaceRollForce = 250.0f;
	// 액터 표면 접선 힘 배율. 높을수록 몸 표면 방향으로 더 끌림.
	constexpr float ActorWakeSurfaceTangentSpeedScale = 0.35f;
	// 액터 표면 난류 힘(cm/s). 높을수록 표면 근처 잔흔이 거칠어짐.
	constexpr float ActorWakeSurfaceNoiseForce = 100.0f;
	// 액터 뒤 wake 최소 회전 힘(cm/s). 높을수록 약한 후류의 회전 증가.
	constexpr float ActorWakeTrailMinRollForce = 120.0f;
	// 액터 뒤 wake 최대 회전 힘(cm/s). 높을수록 강한 후류의 회전 상한 증가.
	constexpr float ActorWakeTrailMaxRollForce = 320.0f;
	// 액터 뒤 wake street 힘 배율. 높을수록 교번 후류 회전 증가.
	constexpr float ActorWakeStreetForceScale = 0.4f;
	// 액터 전면 밀림 힘 배율. 높을수록 액터 앞쪽 연막 밀림 증가.
	constexpr float ActorWakeFrontPushScale = 0.1f;
	// 공기 상호작용 회전 기본 힘(cm/s). 높을수록 기본 회전 반응 증가.
	constexpr float AirInteractionRollBaseForce = 120.0f;
	// 공기 상호작용 접선 속도 힘 배율. 높을수록 접선 방향 흐름 증가.
	constexpr float AirInteractionTangentialSpeedScale = 0.4f;
	// 공기 상호작용 난류 힘(cm/s). 높을수록 액터 주변 난류 증가.
	constexpr float AirInteractionCurlNoiseForce = 200.0f;
	// 와류 confinement 힘 배율. 높을수록 회전 구조 복원력 증가.
	constexpr float VorticityConfinementForceScale = 26.0f;
	// 난류 band 최소 힘(cm/s). 높을수록 약한 난류의 힘 하한 증가.
	constexpr float TurbulenceBandMinForce = 180.0f;
	// 난류 band 최대 힘(cm/s). 높을수록 강한 난류의 힘 상한 증가.
	constexpr float TurbulenceBandMaxForce = 500.0f;
	// 난류 band curl 민감도. 높을수록 작은 curl에도 강한 난류 적용.
	constexpr float TurbulenceCurlMagnitudeScale = 0.2f;
	// 주변 공기 난류 힘(cm/s). 높을수록 이벤트가 없는 영역의 움직임 증가.
	constexpr float AmbientAirCurlForce = 100.0f;
	// 와류 delta 속도 최소 상한(cm/s).
	constexpr float VorticityDeltaSpeedMin = 160.0f;
	// 와류 delta 속도 강도 상한 배율(cm/s).
	constexpr float VorticityDeltaSpeedStrengthScale = 420.0f;
	// 액터 공기 흐름 전체 강도. 높을수록 액터 후류 영향 증가.
	constexpr float ActorAirflowStrength = 0.5f;
	// 액터 공기 흐름 시작 최소 속도(cm/s). 높을수록 느린 액터 반응 감소.
	constexpr float ActorAirflowMinSpeed = 10.0f;
	// 액터 공기 흐름 최대 반응 속도(cm/s). 낮을수록 최대 반응에 빨리 도달.
	constexpr float ActorAirflowFullSpeed = 300.0f;
	// 액터 반경 대비 공기 흐름 반경 배율. 높을수록 영향 범위 증가.
	constexpr float ActorAirflowRadiusScale = 1.0f;
	// 액터 공기 흐름 반경 배율 최소값. 낮을수록 작은 액터 영향 보존.
	constexpr float ActorAirflowRadiusScaleMin = 0.1f;
	// 액터 공기 흐름 최대 반응 속도 최소 간격(cm/s). 높을수록 반응 구간 안정성과 최대 반응 도달 속도 감소.
	constexpr float ActorAirflowFullSpeedMinGap = 1.0f;
	// 액터 앞쪽 압축 공기 흐름 강도. 높을수록 전방 밀림 증가.
	constexpr float ActorAirflowFrontStrength = 1.5f;
	// 액터 측면 공기 흐름 강도. 높을수록 측면 벌어짐 증가.
	constexpr float ActorAirflowSideStrength = 0.3f;
	// 액터 뒤쪽 후류 강도. 높을수록 뒤쪽 끌림 증가.
	constexpr float ActorAirflowWakeStrength = 0.6f;
	// 액터 이동이 만드는 와류 강도. 높을수록 후류 회전 증가.
	constexpr float ActorAirflowVortexStrength = 0.25f;

	// 상호작용 이벤트 한도

	// 연막 하나가 한 프레임에 받을 GPU 이벤트 최대 개수. 높을수록 반응 품질과 비용 증가.
	constexpr int32 MaxGPUEventsPerSmokePerFrame = 32;
	// 셰이더 이벤트 버퍼 최대 개수. 높을수록 많은 이벤트 처리와 버퍼 비용 증가.
	constexpr int32 MaxShaderEventCount = 48;
	// 시뮬레이션에 넘길 총알 이벤트 최대 개수. 높을수록 총알 반응 품질과 비용 증가.
	constexpr int32 MaxSimulationBulletEventCount = 24;
	// 시뮬레이션에 넘길 폭발 이벤트 최대 개수. 높을수록 중첩 폭발 반응과 비용 증가.
	constexpr int32 MaxSimulationExplosionEventCount = 8;
	// 시뮬레이션에 넘길 액터 이벤트 최대 개수. 높을수록 액터 반응 품질과 비용 증가.
	constexpr int32 MaxSimulationActorEventCount = 16;
	// 볼텍스 파이프라인에 넘길 이벤트 최대 개수. 높을수록 와류 반응 품질과 비용 증가.
	constexpr int32 MaxSimulationVortexEventCount = 16;
	// 시뮬레이션 이벤트 최소 세기. 낮을수록 약한 이벤트 보존과 pass 비용 증가.
	constexpr float SimulationEventMinStrength = 0.001f;
	// 셰이더 이벤트 루프 최대 개수. 높을수록 동시 이벤트 처리량과 비용 증가.
	constexpr int32 ShaderEventLoopMaxCount = MaxShaderEventCount;
	// 이벤트 반응 델타 상한(초). 높을수록 저프레임 충격이 강해짐.
	constexpr float SimulationEventDeltaSecondsMax = 1.0f / 15.0f;
	// 연막 하나가 틱당 처리할 총알 트레이스 최대 개수. 높을수록 정확도와 CPU 비용 증가.
	constexpr int32 MaxBulletTracesPerSmokePerTick = 12;
	// 연막 하나가 유지할 폭발 충격 최대 개수. 높을수록 중첩 충격 품질과 비용 증가.
	constexpr int32 MaxActiveExplosionImpulsesPerSmoke = 8;
	// 액터 상호작용 이벤트 최대 생성 개수. 높을수록 액터 반응 품질과 비용 증가.
	constexpr int32 MaxActorInteractionEventsPerTick = 16;

	// 총알 후류

	// 총알이 지우는 연막 반경(cm). 높을수록 구멍 크기 증가.
	constexpr float BulletClearRadius = 32.0f;
	// 총알 구멍 반경 랜덤 최소 배율. 높을수록 가장 작은 구멍 크기 증가.
	constexpr float BulletClearRadiusRandomMin = 0.95f;
	// 총알 구멍 반경 랜덤 최대 배율. 높을수록 가장 큰 구멍 크기 증가.
	constexpr float BulletClearRadiusRandomMax = 1.05f;
	// 총알 후류 강도 랜덤 최소 배율. 높을수록 가장 약한 후류 강도 증가.
	constexpr float BulletWakeStrengthRandomMin = 0.9f;
	// 총알 후류 강도 랜덤 최대 배율. 높을수록 가장 강한 후류 강도 증가.
	constexpr float BulletWakeStrengthRandomMax = 1.0f;
	// 총알 후류가 보이는 최대 시간(초). 높을수록 흔적 지속 증가.
	constexpr float BulletWakeMaxVisibleLife = 0.2f;
	// 총알 구멍이 풀리는 시간(초). 높을수록 구멍 복구가 느림.
	constexpr float BulletWakeReleaseDuration = 0.5f;
	// 총알 주변 공기 통로 유지 시간(초). 높을수록 통로 영향 지속 증가.
	constexpr float BulletWakeSinkLife = 0.2f;
	// 총알 주변 공기 통로 강도. 높을수록 통로 밀도 제거 증가.
	constexpr float BulletWakeSinkStrength = 0.15f;
	// 총알 진행 방향 속도 충격 강도(cm/s). 높을수록 wake 흐름 증가.
	constexpr float BulletWakeImpulseStrength = 90.0f;
	// 총알 구멍 가장자리 부드러움. 높을수록 경계가 부드럽고 넓어짐.
	constexpr float BulletWakeCutoutFeather = 1.5f;
	// 총알 wake 수명 최소값(초).
	constexpr float BulletWakeMinLifeSeconds = 0.05f;
	// 총알 구멍 feather 최소값.
	constexpr float BulletWakeCutoutFeatherMin = 0.2f;
	// 총알 wake 유지 코어 내부 반경 배율. 높을수록 완전히 열린 코어 범위 증가.
	constexpr float BulletWakeHoldCoreInnerRadiusScale = 0.25f;
	// 총알 wake 유지 코어 외부 반경 배율. 높을수록 부드러운 복구 전이 범위 증가.
	constexpr float BulletWakeHoldCoreOuterRadiusScale = 1.5f;

	// 폭발 충격

	// 폭발 이벤트 안전 최소 반경(cm). 실제 영향 반경은 게임/네트워크에서 전달된 값을 사용한다.
	constexpr float ExplosionShockRadius = 1.0f;
	// 폭발 속도 충격 유지 시간(초). 높을수록 충격 지속 증가.
	constexpr float ExplosionImpulseDuration = 1.0f;
	// 폭발 반경 대비 외부 공기장 영향 반경 배율. 높을수록 폭발 반경 밖 연막 밀림 범위 증가.
	constexpr float ExplosionInfluenceRadiusScale = 1.5f;
	// 폭발 바깥 방향 속도 강도(cm/s). 높을수록 연막 밀림 증가.
	constexpr float ExplosionOutwardStrength = 800.0f;

	// 액터 상호작용

	// 액터와 연막 상호작용 샘플 빈도(Hz). 높을수록 반응 품질과 비용 증가.
	constexpr float ActorInteractionHz = 10.0f;
	// 액터 밀기 이벤트 최소 속도(cm/s). 높을수록 약한 반응 감소.
	constexpr float ActorPushVelocityThreshold = 10.0f;
	// 액터 밀기 반응 시작 속도 배율. 높을수록 반응 시작이 늦어짐.
	constexpr float ActorPushResponseStartSpeedScale = 0.15f;
	// 액터 속도 반응 최대 기준 속도(cm/s). 낮을수록 최대 반응에 빨리 도달.
	constexpr float ActorPushFullResponseSpeed = 800.0f;
	// 프리미티브 크기 대비 액터 반응 반경 배율. 높을수록 영향 반경 증가.
	constexpr float ActorPrimitiveRadiusScale = 0.45f;

	// 연막 broadphase

	// 연막 공간 해시 셀 크기(cm). 높을수록 broadphase 비용 감소, 후보 수 증가.
	constexpr float SmokeSpatialCellSize = 2800.0f;
	// 선형 broadphase를 사용할 최대 연막 수. 높을수록 작은 집합의 해시 구성 비용 감소.
	constexpr int32 SmokeBroadphaseLinearScanMaxCount = 8;

	// 렌더러 시뮬레이션 목표 빈도(Hz). 높을수록 반응 품질과 비용 증가.
	constexpr float SimulationHz = 30.0f;
	// 렌더 프레임 델타 누적 상한(초). 높을수록 hitch 복구량과 프레임당 시뮬레이션 비용 증가.
	constexpr float SimulationFrameDeltaSecondsMax = 1.0f / 15.0f;
	// 한 렌더 프레임의 최대 시뮬레이션 substep 수. 높을수록 hitch 복구 속도와 프레임 비용 증가.
	constexpr int32 MaxSimulationSubstepsPerFrame = 4;

	// 이벤트 우선순위

	// 이벤트 우선순위 최소 강도. 높을수록 약한 이벤트 정렬 영향 증가.
	constexpr float EventPriorityMinStrength = 0.01f;
	// 이벤트 우선순위 최소 나이 가중치. 높을수록 오래된 이벤트 보존 증가.
	constexpr float EventPriorityMinAgeWeight = 0.05f;
	// 이벤트 반경 우선순위 기준 반경(cm). 낮을수록 큰 이벤트 우선순위 증가.
	constexpr float EventPriorityRadiusDivisor = 200.0f;
	// 이벤트 반경 우선순위 최소값.
	constexpr float EventPriorityRadiusMin = 0.5f;
	// 이벤트 반경 우선순위 최대값.
	constexpr float EventPriorityRadiusMax = 4.0f;
	// 폭발 이벤트 우선순위 배율. 높을수록 폭발 이벤트 보존 증가.
	constexpr float ExplosionEventPriorityWeight = 1.55f;
	// 액터 이벤트 우선순위 배율. 높을수록 액터 이벤트 보존 증가.
	constexpr float ActorEventPriorityWeight = 1.3f;
	// 생성 플룸 이벤트 우선순위 배율. 높을수록 생성 소스 보존 증가.
	constexpr float PlumeEventPriorityWeight = 1.45f;
	// 총알 이벤트 우선순위 배율. 높을수록 총알 이벤트 보존 증가.
	constexpr float BulletEventPriorityWeight = 1.15f;
	// 지속 상호작용 이벤트 최소 유지 시간(초). 높을수록 짧은 이벤트 보존과 잔류 반응 증가.
	constexpr float ActiveImpulseMinDurationSeconds = 0.01f;

	// 반해상도 렌더링 및 양방향 업샘플링

	// 양방향 업샘플 깊이 차이 민감도. 높을수록 불투명 경계 보존과 계단 현상 위험 증가.
	constexpr float BilateralDepthSensitivity = 10000.0f;
}
