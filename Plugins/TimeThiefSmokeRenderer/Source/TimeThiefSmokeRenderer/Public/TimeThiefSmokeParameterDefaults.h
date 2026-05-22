#pragma once

#include "CoreMinimal.h"

namespace TimeThiefSmokeParameterDefaults
{
	// 범위, 수명

	// 실제 시뮬레이션 연막 박스 반경(cm).
	inline FVector GetSmokeBoundsExtent()
	{
		return FVector(800.0, 800.0, 800.0);
	}

	// 렌더링 박스에 더하는 여유 반경(cm). 높을수록 가장자리 잘림 감소, raymarch 범위 증가.
	inline FVector GetRenderBoundsPadding()
	{
		return FVector(400.0, 400.0, 400.0);
	}

	// 연막 지속 시간(초).
	constexpr float SmokeDuration = 30.0f;
	// 연막 페이드아웃 시간(초).
	constexpr float SmokeFadeOutDuration = 5.0f;
	// 초기 연막 밀도.
	constexpr float InitialDensity = 1.5f;

	// 생성 플룸

	// 생성 직후 밀도와 속도 소스를 넣는 시간(초).
	constexpr float PlumeEmissionDuration = 2.5f;
	// 생성 소스 반경(cm). 높을수록 초기 연막 덩어리가 커짐.
	constexpr float PlumeSourceRadius = 100.0f;
	// 초기 연막 수평 확산 속도(cm/s).
	constexpr float PlumeExpansionVelocity = 250.0f;
	// 초기 연막 상승 속도(cm/s).
	constexpr float PlumeRiseVelocity = 150.0f;

	// 장애물 마스크

	// 정적 월드 장애물 마스크 사용 여부. 끄면 비용 감소, 벽 차단 품질 감소.
	constexpr bool bUseStaticObstacleMask = true;
	// 장애물 마스크 3D 텍스처 축 해상도. 높을수록 충돌 품질과 비용 증가.
	constexpr int32 ObstacleMaskResolution = 24;
	// 장애물 검사 박스 최소 여유 거리(cm). 높을수록 얇은 장애물 검출 증가, 과차단 위험 증가.
	constexpr float ObstacleMaskInflation = 2.0f;
	// 장애물 마스크 복셀 검사 박스의 셀 반경 비율. 높을수록 누락 감소, 과차단 증가.
	constexpr float ObstacleMaskCellFootprintRatio = 0.25f;
	// 활성 경계 셀 장애물 검사 박스의 셀 반경 비율. 높을수록 차단 안정성 증가, 활성 영역 감소.
	constexpr float BoundsCellObstacleFootprintRatio = 0.1f;
	// 생성 원점 주변 장애물 차단 제거 반경 배율. 높을수록 원점 근처 생성 안정성 증가, 장애물 침범 증가.
	constexpr float ObstacleSourceClearRadiusScale = 2.0f;
	// 장애물 마스크 변경 블렌딩 시간(초). 높을수록 변화가 부드럽고 반응이 느림.
	constexpr float ObstacleMaskBlendDuration = 0.25f;

	// 활성 경계 셀

	// 내부 경계 셀로 연막 활성 영역을 제한할지 여부. 켜면 장애물 근처 불필요한 연산 감소.
	constexpr bool bUseBoundsCellCluster = true;
	// 내부 경계 셀 격자 크기. 높을수록 활성 영역 형태 품질과 비용 증가.
	inline FIntVector GetBoundsCellGrid()
	{
		return FIntVector(6, 6, 4);
	}
	// 동시에 활성화할 내부 경계 셀 최대 개수. 높을수록 퍼질 수 있는 영역과 비용 증가.
	constexpr int32 MaxActiveBoundsCells = 48;
	// 폭발 방향으로 활성 경계 중심을 이동시키는 배율. 높을수록 폭발 반응 범위 이동 증가.
	constexpr float ExplosionBoundsShiftScale = 0.5f;
	// 활성 경계 중심 이동의 최대 비율. 높을수록 중심 이동 허용 범위 증가.
	constexpr float BoundsClusterMaxOffsetRatio = 0.42f;
	// 활성 경계 가장자리 페더 셀 수. 높을수록 경계 전이가 부드러움.
	constexpr float ActiveBoundsOpenFeatherCells = 1.35f;

	// 시뮬레이션 격자

	// 희소 MAC 격자 백엔드 기본 사용 여부. 끄면 dense 경로 사용.
	constexpr bool bUseSparseMacSimulationByDefault = true;
	// 멀티그리드 압력 풀이 기본 사용 여부. 켜면 압력 품질과 패스 수 증가.
	constexpr bool bUseMultigridPressureByDefault = false;
	// 연막 3D 격자 기준 축 해상도. 높을수록 시뮬레이션 품질과 비용 증가.
	constexpr int32 SmokeGridResolution = 128;
	// 컴퓨트 셰이더 스레드 그룹 한 변 크기. 셰이더 numthreads와 맞아야 함.
	constexpr int32 SmokeThreadGroupSize = 4;
	// 격자 재할당 축 정렬 단위. 높을수록 재할당 감소, 메모리 여유 증가.
	constexpr int32 SmokeGridAllocationQuantum = 32;
	// Jacobi 압력 반복 횟수. 높을수록 압력 품질과 비용 증가.
	constexpr int32 PressureIterations = 4;
	// 희소 브릭 한 변의 복셀 수. 높을수록 관리 비용 감소, 빈 공간 낭비 증가.
	constexpr int32 SmokeBrickSize = 32;
	// 희소 아틀라스 활성 브릭 최대 개수. 높을수록 넓은 연막 지원과 VRAM 비용 증가.
	constexpr int32 MaxActiveSmokeBricks = 1024;
	// 희소 브릭 활성화 최소 속도(cm/s). 높을수록 비용 감소, 약한 움직임 손실 증가.
	constexpr float SparseVelocityActiveThreshold = 120.0f;

	// 멀티그리드 압력

	// 멀티그리드 최대 레벨 수. 높을수록 큰 구조 압력 품질과 비용 증가.
	constexpr int32 MultigridMaxLevelCount = 4;
	// 멀티그리드 V-cycle 반복 횟수. 높을수록 압력 품질과 비용 증가.
	constexpr int32 MultigridCycleCount = 2;
	// 멀티그리드 하강 smoothing 반복 횟수. 높을수록 수렴 품질과 비용 증가.
	constexpr int32 MultigridPreSmoothPassCount = 2;
	// 멀티그리드 상승 smoothing 반복 횟수. 높을수록 수렴 품질과 비용 증가.
	constexpr int32 MultigridPostSmoothPassCount = 2;
	// 멀티그리드 최저 해상도 smoothing 반복 횟수. 높을수록 저주파 압력 품질과 비용 증가.
	constexpr int32 MultigridCoarsestSmoothPassCount = 12;

	// 렌더링 raymarch

	// 기본 raymarch 샘플 수. 높을수록 렌더 품질과 비용 증가.
	constexpr int32 RenderStepCount = 16;
	// adaptive raymarch 최대 샘플 수. 높을수록 두꺼운 연막 품질과 비용 증가.
	constexpr int32 RenderMaxStepCount = 128;
	// raymarch 목표 스텝 길이의 voxel 배율. 높을수록 비용 감소, 디테일 감소.
	constexpr float RenderStepVoxelScale = 1.25f;
	// 멀티 연막 composite tile 한 변 크기(px). 높을수록 타일 관리 비용 감소, culling 정밀도 감소.
	constexpr int32 CompositeTileSize = 32;
	// 한 번에 합성할 연막 슬롯 최대 개수. 높을수록 겹친 연막 처리량과 비용 증가.
	constexpr int32 MaxCompositeSmokeSlots = 7;
	// fullscreen composite 전환 화면 면적 비율. 낮을수록 fullscreen 경로를 더 빨리 사용.
	constexpr float CompositeFullscreenAreaThreshold = 0.58f;
	// 연막 screen rect 여유 픽셀(px). 높을수록 가장자리 누락 감소, 합성 면적 증가.
	constexpr int32 CompositeScreenRectPadding = 12;

	// 빛과 그림자

	// 연막 흡수 계수. 높을수록 더 어둡고 불투명하게 보임.
	constexpr float Extinction = 3.0f;
	// 산란 색 반사 비율(0~1). 높을수록 밝게 보임.
	constexpr float ScatteringAlbedo = 0.75f;
	// 전방 산란 방향성(-1~1). 높을수록 빛 방향 하이라이트 증가.
	constexpr float ScatteringAnisotropy = 0.2f;
	// 자체 그림자 기준 빛 방향.
	inline FVector3f GetSelfShadowLightDirection()
	{
		return FVector3f(-0.45f, -0.25f, 0.86f).GetSafeNormal();
	}
	// 자체 그림자 적용 강도(0~1). 높을수록 연막 내부 그림자 증가.
	constexpr float SelfShadowStrength = 0.55f;
	// 자체 그림자 흡수 계수. 높을수록 그림자가 진해짐.
	constexpr float SelfShadowExtinction = 1.1f;
	// 자체 그림자 샘플 수. 높을수록 그림자 품질과 비용 증가.
	constexpr int32 SelfShadowStepCount = 4;
	// 자체 그림자 샘플 간격(cm). 높을수록 넓은 그림자, 낮을수록 세밀한 그림자.
	constexpr float SelfShadowStepLength = 95.0f;

	// 렌더링 디테일

	// 렌더 노이즈 공간 스케일. 높을수록 패턴이 촘촘해짐.
	constexpr float RenderNoiseScale = 0.1f;
	// 렌더 노이즈 강도. 높을수록 밀도 디테일 증가.
	constexpr float RenderNoiseStrength = 1.0f;
	// 렌더 노이즈 시간 스케일. 높을수록 노이즈 움직임 증가.
	constexpr float RenderNoiseTimeScale = 0.05f;
	// 필라멘트 공간 스케일. 높을수록 필라멘트가 촘촘해짐.
	constexpr float RenderFilamentScale = 1.0f;
	// 필라멘트 강도. 높을수록 선형 디테일 증가.
	constexpr float RenderFilamentStrength = 2.0f;
	// 필라멘트 대비. 높을수록 선명도 증가, aliasing 위험 증가.
	constexpr float RenderFilamentContrast = 5.0f;
	// 필라멘트 도메인 워프 강도. 높을수록 꼬임 디테일 증가.
	constexpr float RenderFilamentWarpStrength = 4.0f;

	// 이류와 감쇠

	// 초당 밀도 자연 감소율. 높을수록 연막이 빨리 옅어짐.
	constexpr float DensityDissipation = 0.05f;
	// 초당 속도 감쇠율. 높을수록 움직임이 빨리 안정됨.
	constexpr float VelocityDamping = 0.4f;
	// MacCormack 이류 사용 여부. 켜면 이류 품질과 비용 증가.
	constexpr bool bUseMacCormackAdvection = true;
	// 약한 영역에서 MacCormack 보정을 줄일지 여부. 켜면 비용 감소, 약한 디테일 감소.
	constexpr bool bUseAdaptiveMacCormack = true;

	// 와류와 흔들림

	// 기본 와류 강도. 높을수록 소용돌이 움직임 증가.
	constexpr float VorticityStrength = 0.45f;
	// 와류 보존 강도. 높을수록 회전 구조 유지와 비용 증가.
	constexpr float VorticityConfinementStrength = 1.5f;
	// 난류 힘 강도. 높을수록 불규칙 움직임 증가.
	constexpr float TurbulenceStrength = 0.5f;
	// 주변 공기 흐름 반응 강도. 높을수록 외부 힘 반응 증가.
	constexpr float AirInteractionStrength = 0.5f;
	// 자체 흔들림 시간 스케일. 높을수록 흔들림 변화 속도 증가.
	constexpr float SelfWobbleTimeScale = 0.025f;
	// 자체 흔들림 속도 스케일. 높을수록 내부 움직임 증가.
	constexpr float SelfWobbleVelocityScale = 0.25f;
	// 이벤트 주변 자체 흔들림 힘 배율. 높을수록 이벤트 반응 증가.
	constexpr float SelfWobbleForceScale = 0.5f;
	// 와류 입자 자체 흔들림 배율. 높을수록 와류 입자 움직임 증가.
	constexpr float SelfWobbleParticleScale = 0.25f;
	// 이벤트가 만드는 국소 와류 강도. 높을수록 총알/폭발/액터 반응 증가.
	constexpr float EventVortexStrength = 0.5f;
	// 와류 입자 수. 높을수록 디테일과 비용 증가.
	constexpr int32 VortexParticleCount = 48;
	// 와류 입자 최대 수. 높을수록 설정 허용 범위와 버퍼 비용 증가.
	constexpr int32 MaxVortexParticleCount = 128;
	// 와류 입자 수명(초). 높을수록 와류 디테일 지속 시간 증가.
	constexpr float VortexParticleLifeSeconds = 2.0f;
	// 와류 입자 속도 주입 강도(cm/s). 높을수록 국소 회전 움직임 증가.
	constexpr float VortexParticleStrength = 70.0f;
	// 와류 입자 영향 반경(cm). 높을수록 영향 범위와 비용 증가.
	constexpr float VortexParticleSplatRadius = 150.0f;
	// 와류 입자 중심 코어 반경(cm). 높을수록 중심 영향 범위 증가.
	constexpr float VortexParticleCoreRadius = 32.0f;
	// 밀도 경계 와류 입자 생성 민감도. 높을수록 경계 와류 생성 증가.
	constexpr float VortexDensityGradientScale = 4.0f;
	// 와류 입자 업데이트 간격(초). 높을수록 비용 감소, 반응 지연 증가.
	constexpr float VortexSubstepIntervalSeconds = 1.0f / 10.0f;
	// 와류 입자를 브릭 주변으로 제한할지 여부. 켜면 비용 감소.
	constexpr bool bUseVortexBrickBins = true;

	// 액터 워프와 공기 흐름

	// 액터 통과 워프 꼬리 초기 강도. 높을수록 시각 흔적 증가.
	constexpr float WarpTrailIntensity = 5.0f;
	// 워프 꼬리 초당 감쇠율. 높을수록 흔적이 빨리 사라짐.
	constexpr float WarpTrailDecayRate = 1.5f;
	// 액터 반경 대비 워프 꼬리 반경 배율. 높을수록 흔적 폭 증가.
	constexpr float WarpTrailRadiusScale = 0.5f;
	// 액터 속도와 반경 대비 워프 꼬리 길이 배율. 높을수록 흔적 길이 증가.
	constexpr float WarpTrailLengthScale = 7.0f;
	// 액터 워프 누적 밀도 배율. 높을수록 반복 통과 흔적 증가.
	constexpr float ActorWarpDensityAccumulationScale = 1.5f;
	// 액터 워프 누적값 감쇠 시간(초). 높을수록 누적 흔적 지속 증가.
	constexpr float ActorWarpAccumulationDecaySeconds = 0.3f;
	// 액터 워프 생성 후 남길 누적 비율. 높을수록 다음 흔적에 더 많이 누적됨.
	constexpr float ActorWarpEmissionRemainder = 0.2f;
	// 액터 공기 흐름 전체 강도. 높을수록 액터 후류 영향 증가.
	constexpr float ActorAirflowStrength = 1.0f;
	// 액터 공기 흐름 시작 최소 속도(cm/s). 높을수록 느린 액터 반응 감소.
	constexpr float ActorAirflowMinSpeed = 10.0f;
	// 액터 공기 흐름 최대 반응 속도(cm/s). 낮을수록 최대 반응에 빨리 도달.
	constexpr float ActorAirflowFullSpeed = 320.0f;
	// 액터 반경 대비 공기 흐름 반경 배율. 높을수록 영향 범위 증가.
	constexpr float ActorAirflowRadiusScale = 1.0f;
	// 액터 앞쪽 압축 공기 흐름 강도. 높을수록 전방 밀림 증가.
	constexpr float ActorAirflowFrontStrength = 2.0f;
	// 액터 측면 공기 흐름 강도. 높을수록 측면 벌어짐 증가.
	constexpr float ActorAirflowSideStrength = 0.3f;
	// 액터 뒤쪽 후류 강도. 높을수록 뒤쪽 끌림 증가.
	constexpr float ActorAirflowWakeStrength = 0.7f;
	// 액터 이동이 만드는 와류 강도. 높을수록 후류 회전 증가.
	constexpr float ActorAirflowVortexStrength = 0.3f;

	// 상호작용 이벤트 한도

	// 연막 하나가 한 프레임에 받을 GPU 이벤트 최대 개수. 높을수록 반응 품질과 비용 증가.
	constexpr int32 MaxGPUEventsPerSmokePerFrame = 64;
	// 셰이더 이벤트 버퍼 최대 개수. 높을수록 많은 이벤트 처리와 버퍼 비용 증가.
	constexpr int32 MaxShaderEventCount = 128;
	// 디버그 이벤트 표시 최대 개수. 높을수록 디버그 정보와 메모리 사용 증가.
	constexpr int32 MaxDebugEventCount = 128;
	// 시뮬레이션에 넘길 총알 이벤트 최대 개수. 높을수록 총알 반응 품질과 비용 증가.
	constexpr int32 MaxSimulationBulletEventCount = 32;
	// 시뮬레이션에 넘길 폭발 이벤트 최대 개수. 높을수록 중첩 폭발 반응과 비용 증가.
	constexpr int32 MaxSimulationExplosionEventCount = 8;
	// 시뮬레이션에 넘길 액터 이벤트 최대 개수. 높을수록 액터 반응 품질과 비용 증가.
	constexpr int32 MaxSimulationActorEventCount = 32;
	// 시뮬레이션에 넘길 힘 이벤트 최대 개수. 높을수록 힘 반응 품질과 비용 증가.
	constexpr int32 MaxSimulationForceEventCount = 32;
	// 연막 하나가 틱당 처리할 총알 트레이스 최대 개수. 높을수록 정확도와 CPU 비용 증가.
	constexpr int32 MaxBulletTracesPerSmokePerTick = 16;
	// 연막 하나가 유지할 폭발 충격 최대 개수. 높을수록 중첩 충격 품질과 비용 증가.
	constexpr int32 MaxActiveExplosionImpulsesPerSmoke = 16;
	// 액터 상호작용 이벤트 최대 생성 개수. 높을수록 액터 반응 품질과 비용 증가.
	constexpr int32 MaxActorInteractionEventsPerTick = 64;

	// 총알 후류

	// 총알이 지우는 연막 반경(cm). 높을수록 구멍 크기 증가.
	constexpr float BulletClearRadius = 30.0f;
	// 총알 구멍 반경 랜덤 최소 배율.
	constexpr float BulletClearRadiusRandomMin = 0.95f;
	// 총알 구멍 반경 랜덤 최대 배율.
	constexpr float BulletClearRadiusRandomMax = 1.2f;
	// 총알 후류 강도 랜덤 최소 배율.
	constexpr float BulletWakeStrengthRandomMin = 0.92f;
	// 총알 후류 강도 랜덤 최대 배율.
	constexpr float BulletWakeStrengthRandomMax = 1.0f;
	// 총알 후류가 보이는 최대 시간(초). 높을수록 흔적 지속 증가.
	constexpr float BulletWakeMaxVisibleLife = 0.25f;
	// 총알 구멍이 풀리는 시간(초). 높을수록 구멍 복구가 느림.
	constexpr float BulletWakeReleaseDuration = 2.0f;
	// 총알 주변 공기 통로 유지 시간(초). 높을수록 통로 영향 지속 증가.
	constexpr float BulletWakeSinkLife = 0.25f;
	// 총알 주변 공기 통로 강도. 높을수록 통로 밀도 제거 증가.
	constexpr float BulletWakeSinkStrength = 0.25f;
	// 총알 진행 방향 속도 충격 강도(cm/s). 높을수록 wake 흐름 증가.
	constexpr float BulletWakeImpulseStrength = 48.0f;
	// 총알 구멍 가장자리 부드러움. 높을수록 경계가 부드럽고 넓어짐.
	constexpr float BulletWakeCutoutFeather = 1.95f;

	// 폭발 충격

	// 폭발 충격 영향 반경(cm). 높을수록 영향 범위와 비용 증가.
	constexpr float ExplosionShockRadius = 700.0f;
	// 폭발 속도 충격 유지 시간(초). 높을수록 충격 지속 증가.
	constexpr float ExplosionImpulseDuration = 1.0f;
	// 폭발 바깥 방향 속도 강도(cm/s). 높을수록 연막 밀림 증가.
	constexpr float ExplosionOutwardStrength = 720.0f;
	// 폭발 즉시 밀도 제거 비율(0~1). 높을수록 제거 강도 증가.
	constexpr float ExplosionDensityClearStrength = 0.25f;

	// 액터 상호작용

	// 액터와 연막 상호작용 샘플 빈도(Hz). 높을수록 반응 품질과 비용 증가.
	constexpr float ActorInteractionHz = 30.0f;
	// 액터 밀기 이벤트 최소 속도(cm/s). 높을수록 약한 반응 감소.
	constexpr float ActorPushVelocityThreshold = 10.0f;
	// 액터 밀기 반응 시작 속도 배율. 높을수록 반응 시작이 늦어짐.
	constexpr float ActorPushResponseStartSpeedScale = 0.35f;
	// 액터 속도 반응 최대 기준 속도(cm/s). 낮을수록 최대 반응에 빨리 도달.
	constexpr float ActorPushFullResponseSpeed = 600.0f;
	// 액터 경로 밀도에서 최대 샘플을 섞는 비율. 높을수록 진한 영역 반응 증가.
	constexpr float ActorPushMaxDensityWeight = 0.65f;
	// 액터 점유 반응 최소 밀도. 높을수록 약한 연막 반응 감소.
	constexpr float ActorPushOccupancyMinDensity = 0.015f;
	// 액터 점유 반응 최소 강도. 높을수록 느린 액터 반응 증가.
	constexpr float ActorPushOccupancyMinStrength = 0.12f;
	// 액터 점유 반응 최대 강도. 높을수록 밀도 기반 반응 증가.
	constexpr float ActorPushOccupancyMaxStrength = 0.32f;
	// 액터 이동 거리 게이트 반경 배율. 높을수록 warp 누적 시작이 느려짐.
	constexpr float ActorPushTravelGateRadiusScale = 0.65f;
	// 액터 이동 거리 게이트 최소 거리(cm).
	constexpr float ActorPushTravelGateMinDistance = 8.0f;
	// 액터 워프 누적 최소 강도. 높을수록 느린 움직임도 흔적을 더 남김.
	constexpr float ActorWarpDepositMinStrength = 0.25f;
	// 프리미티브 크기 대비 액터 반경 배율. 높을수록 액터 영향 반경 증가.
	constexpr float ActorPrimitiveRadiusScale = 0.35f;

	// 액터 밀도 샘플

	// 자연 연막 거리 세로 압축 비율. 낮을수록 위아래 밀도 샘플 범위 감소.
	constexpr float ActorDensityNaturalVerticalScale = 0.86f;
	// 자연 연막 밀도 감쇠 시작 거리. 높을수록 내부 밀도 샘플 범위 증가.
	constexpr float ActorDensityNaturalFadeStart = 0.18f;
	// 자연 연막 밀도 감쇠 폭. 높을수록 경계 밀도 변화가 부드러움.
	constexpr float ActorDensityNaturalFadeWidth = 0.78f;
	// 렌더 박스 거리 세로 압축 비율. 낮을수록 위아래 경계 페이드가 강해짐.
	constexpr float ActorDensityRenderVerticalScale = 0.9f;
	// 렌더 박스 밀도 감쇠 시작 거리. 높을수록 렌더 경계까지 반응 유지.
	constexpr float ActorDensityRenderFadeStart = 0.86f;
	// 렌더 박스 밀도 감쇠 폭. 높을수록 경계 반응이 부드러움.
	constexpr float ActorDensityRenderFadeWidth = 0.16f;
	// 생성 직후 밀도 샘플 상승 시간(초). 높을수록 초기 액터 반응이 천천히 켜짐.
	constexpr float ActorDensityEmissionWarmupSeconds = 0.35f;
	// 초기 밀도를 샘플 스케일로 바꾸는 나눗값. 높을수록 액터 밀도 샘플링 강도 감소.
	constexpr float ActorDensityScaleDivisor = 3.2f;
	// 액터 밀도 샘플 최종 보정 배율. 높을수록 액터 반응 증가.
	constexpr float ActorDensitySampleBoost = 1.35f;

	// 클러스터와 broadphase

	// 연막 클러스터 결합 경계 확장 비율. 높을수록 클러스터 결합이 쉬워짐.
	constexpr float SmokeClusterBoundsExpansionRatio = 0.10f;
	// 연막 클러스터 결합 최소 확장 거리(cm). 높을수록 떨어진 연막도 결합하기 쉬움.
	constexpr float SmokeClusterMinExpansionCm = 120.0f;
	// 연막 클러스터 유지 경계 확장 비율. 높을수록 기존 결합 유지가 쉬워짐.
	constexpr float SmokeClusterReleaseBoundsExpansionRatio = 0.26f;
	// 연막 클러스터 유지 최소 확장 거리(cm). 높을수록 기존 결합 유지 범위 증가.
	constexpr float SmokeClusterReleaseMinExpansionCm = 320.0f;
	// 연막 공간 해시 셀 크기(cm). 높을수록 broadphase 비용 감소, 후보 수 증가.
	constexpr float SmokeSpatialCellSize = 2400.0f;

	// 런타임 디버그 기본값

	// 디버그 뷰 기본 모드. 0은 비활성.
	constexpr int32 DebugViewDefault = 0;
	// 합성 scissor 기본 사용 여부. 켜면 합성 면적과 비용 감소.
	constexpr int32 bUseCompositeScissorByDefault = 1;
	// 프로파일 로그 기본 모드. 0은 비활성.
	constexpr int32 ProfileModeDefault = 0;
	// 빠른 필라멘트 렌더링 기본 사용 여부. 켜면 비용 감소, 디테일 감소.
	constexpr int32 bUseFastFilamentByDefault = 1;
	// 렌더러 시뮬레이션 목표 빈도(Hz). 높을수록 반응 품질과 비용 증가.
	constexpr float SimulationHz = 30.0f;
	// 프로파일 로그 간격(프레임). 높을수록 로그 빈도 감소.
	constexpr uint32 ProfileLogIntervalFrames = 60u;

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
	constexpr float ExplosionEventPriorityWeight = 1.4f;
	// 액터 이벤트 우선순위 배율. 높을수록 액터 이벤트 보존 증가.
	constexpr float ActorEventPriorityWeight = 1.2f;
	// 생성 플룸 이벤트 우선순위 배율. 높을수록 생성 소스 보존 증가.
	constexpr float PlumeEventPriorityWeight = 1.35f;
	// 총알 이벤트 우선순위 배율. 높을수록 총알 이벤트 보존 증가.
	constexpr float BulletEventPriorityWeight = 1.1f;

	// 디버그 표시

	// 연막 경계 디버그 표시 여부. 켜면 디버그 draw 비용 증가.
	constexpr bool bDrawDebugBounds = false;
	// 디버그 연막 반경(cm).
	constexpr float SmokeDebugRadius = 450.0f;
	// 디버그 연막 유지 시간(초).
	constexpr float SmokeDebugDuration = 12.0f;
	// 디버그 원 세그먼트 수. 높을수록 원 품질과 draw 비용 증가.
	constexpr int32 SmokeDebugSegments = 32;
	// 디버그 색상.
	inline FColor GetSmokeDebugColor()
	{
		return FColor::Silver;
	}
}
