# TimeThief 연막 잔여 최적화 적용 및 성능 테스트 결과

## 테스트 환경

| 항목 | 값 |
|---|---|
| 날짜 | 2026-07-16 |
| 엔진 / RHI | Unreal Engine 5.7 / D3D12 SM6 |
| GPU | NVIDIA GeForce RTX 4080 |
| 빌드 | TimeThiefEditor Win64 Development |
| 렌더링 | 640×360, RenderOffscreen |
| 시간 제어 | 고정 30 FPS (`-UseFixedTimeStep -FPS=30`) |

GPU 수치는 드라이버와 실행 순서의 영향을 받으므로 동일 시나리오의 중앙값, P95, 누적 시간을 함께 비교했다. 필드 동등성은 GPU probe 값으로 확인했다.

## 최종 도입 결정

| 항목 | 최종 상태 | 근거 |
|---|---|---|
| 이벤트 유효 비트 마스크 + 워드 직접 순회 | 기본 ON (`EventBitIteration=1`) | 32발 probe 완전 일치, Advection 중앙값 21.4% 감소, P95 60.4% 감소 |
| 이벤트 Auto 모드 | 사용 가능 (`=2`), 기본값 아님 | 중앙값은 강제 비트 모드와 같지만 P95가 0.0129→0.0228 ms로 증가 |
| 저장 SDF 기반 이진 이웃 마스크 | 실험 경로 유지, 기본 OFF (`FaceOpenStencils=0`) | 시뮬레이션 해상도 기준으로 수정했으나 Curl 중앙값이 0.003→0.004 ms로 증가 |
| 무이벤트 Simulate/Vorticity 퍼뮤테이션 | 제거 | 두 번의 역순 비교 모두 Advection이 느려짐 |
| 소용돌이 brick mask `uint4→uint` | 도입 | 최대 입자 수 32 고정, 메모리 75% 감소, 모든 검증 readback 일치 |
| 진정한 face-aware solver | 이번 변경에서 제외 | Curl 한 단계만 바꾸면 pressure/divergence/projection과 경계 조건이 불일치함 |

## 이벤트 순회 비교

시나리오: `SafeReapplyBulletBurst32`, 각 모드 63개 Advection 샘플.

| 모드 | 중앙값 | P95 | 누적 시간 | 전체 기록 GPU 합계 | probe |
|---|---:|---:|---:|---:|---|
| 0: Legacy full range | 0.0140 ms | 0.0326 ms | 1.022 ms | 15.561 ms | 통과 |
| 1: Set-bit word loop | **0.0110 ms** | **0.0129 ms** | **0.711 ms** | **12.617 ms** | 통과 |
| 2: Auto | 0.0110 ms | 0.0228 ms | 0.862 ms | 17.389 ms | 통과 |

세 모드의 `burst_active`, `burst_recovery` probe JSON은 바이트 단위로 동일했다. 따라서 출력 순서와 결과를 보존하면서 모드 1이 이 비교에서 가장 안정적이었다.

## 저장 SDF 이웃 마스크 비교

초기 구현은 장애물 SDF 16³ 좌표를 시뮬레이션 24³ 좌표로 직접 읽어 큰 필드 차이를 만들었다. 마스크를 시뮬레이션 해상도에서 만들고 SDF를 같은 voxel UV로 삼선형 샘플하도록 수정했다.

| 항목 | SDF 직접 경로 | 이진 마스크 경로 | 차이 |
|---|---:|---:|---:|
| Natural density | 9,810.4297 | 9,827.3086 | +0.172% |
| Displaced density | 1,143.8042 | 1,146.2329 | +0.212% |
| Density centroid 거리 | 기준 | - | 0.832 cm |
| Max velocity | 509.7427 | 504.9808 | -0.934% |
| Density inside obstacle | 102.0660 | 102.1761 | +0.108% |
| Curl 중앙값 | **0.003 ms** | 0.004 ms | +33.3% |
| Curl P95 | **0.00455 ms** | 0.005 ms | +9.9% |

필드 차이는 이 시나리오의 반복 실행 편차와 비슷한 수준까지 줄었지만 성능 이득이 없으므로 기본값은 OFF로 유지한다.

## 무이벤트 퍼뮤테이션 비교

| 경로 | 실행 | 전체 Advection 중앙값 | P95 | Idle 중앙값 |
|---|---:|---:|---:|---:|
| 기존 공용 셰이더 | R1 | **0.008 ms** | **0.0127 ms** | **0.008 ms** |
| 기존 공용 셰이더 | R2 | **0.008 ms** | **0.0130 ms** | **0.008 ms** |
| 무이벤트 퍼뮤테이션 | R1 | 0.009 ms | 0.0217 ms | 0.016 ms |
| 무이벤트 퍼뮤테이션 | R2 | 0.009 ms | 0.0150 ms | 0.014 ms |

코드 제거 효과가 실제 GPU 실행 결과로 이어지지 않았고 오히려 느려져 최종 코드에서는 제거했다.

## 소용돌이 마스크 압축

`VortexParticleCount=24`, `MaxVortexParticleCount=32`를 확인하고 한 brick당 마스크를 16바이트에서 4바이트로 줄였다. 최대값이 32를 넘으면 빌드가 실패하도록 `static_assert`를 추가했다.

| 항목 | 압축 전 | 압축 후 | 결과 |
|---|---:|---:|---|
| 마스크 크기 / brick | 16 B | 4 B | **75% 감소** |
| Reverse BrickMasks 중앙값 | 0.002 ms | 0.002 ms | 동일 |
| Reverse BrickMasks P95 | 0.003 ms | 0.003 ms | 동일 |
| ParticleSplat 중앙값 | 0.004 ms | 0.004 ms | 동일 |
| Legacy/Reverse 검증 | 4 words/brick | 1 word/brick | 기록된 모든 readback mismatch 0 |

## 최종 회귀 테스트

| 시나리오 | `test_valid` | 기대 조건 | GPU pass 샘플 | 기록 GPU 합계 | 결과 |
|---|---|---|---:|---:|---|
| TimeThief.SmokeTest.ScenarioParser | - | 통과 | - | - | 통과 |
| PhaseCostStudy | true | 통과 | 1,968 | 32.571 ms | 통과 |
| ObstacleEnvironmentVariants | true | 통과 | 1,716 | 22.361 ms | 통과 |
| SmokeInteractionVariants | true | 통과 | 1,237 | 31.532 ms | 통과 |
| ActorCollisionVariants | true | 통과 | 1,744 | 34.678 ms | 통과 |
| SafeReapplyBulletBurst32 | true | 통과 | 모드별 동일 | 위 표 참조 | 통과 |

## 추가 개선 계획

| 우선순위 | 항목 | 완료 기준 |
|---:|---|---|
| 1 | 이벤트 모드 0/1 장기 A-B-B-A 재측정 | 500개 이상 유효 이벤트 샘플, P95와 누적 시간 모두 모드 1 우세 확인 |
| 2 | 장애물 마스크의 R32_UINT 대신 R8_UINT 지원 가능성 확인 | 동일 결과와 실제 Curl 감소가 모두 확인될 때만 기본 도입 |
| 3 | face-aware solver 별도 설계 | divergence, pressure, projection, velocity 경계가 동일 face flux를 공유 |
| 4 | 다중 연막·동적 장애물 장기 soak | GPU 오류 0, probe 임계 초과 0, 메모리 증가 없음 |
