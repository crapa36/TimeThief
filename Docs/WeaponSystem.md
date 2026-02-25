# TimeThief 총기 시스템 문서

## 개요
TimeThief 프로젝트의 총기 시스템은 히트스캔 기반의 FPS 무기 시스템으로, 모듈화된 컴포넌트 아키텍처를 사용합니다.

### 주요 설계 원칙
- **Timer 기반 발사**: Tick() 대신 FTimerManager를 사용하여 프레임 독립적인 발사 속도 보장
- **표준 데미지 프레임워크**: UGameplayStatics::ApplyPointDamage()를 통한 언리얼 표준 데미지 시스템 활용
- **슬롯 기반 무기 관리**: 확장 가능한 무기 슬롯 시스템

---

## 클래스 구조

```
AActor
└── ATimeThiefWeaponBase (무기 베이스 클래스)
    └── ATimeThiefRifle (라이플 구현)

UActorComponent
├── UTimeThiefHealthComponent (체력 관리)
└── UTimeThiefPawnExtensionComponent
    └── UTimeThiefPawnCombatComponent (전투 컴포넌트 베이스)
        └── UTimeThiefPlayerCombatComponent (플레이어 전투 컴포넌트)
```

---

## 1. ATimeThiefWeaponBase

**파일 위치:** `Source/TimeThief/Weapon/TimeThiefWeaponBase.h/cpp`

**역할:** 모든 무기의 베이스 클래스. 공통 속성과 애니메이션 레이어 시스템을 제공합니다.

### 변수

| 변수명 | 타입 | 설명 |
|--------|------|------|
| `WeaponTag` | `FGameplayTag` | 무기 식별용 GameplayTag |
| `SocketName` | `FName` | 캐릭터 메시에 부착할 소켓 이름 |
| `WeaponMesh` | `TObjectPtr<USkeletalMeshComponent>` | 무기의 스켈레탈 메시 |
| `EquipAnimLayer` | `TSubclassOf<UAnimInstance>` | 장착 시 링크할 애니메이션 레이어 |
| `EquipMontage` | `TObjectPtr<UAnimMontage>` | 장착 애니메이션 몽타주 |
| `UnequipMontage` | `TObjectPtr<UAnimMontage>` | 해제 애니메이션 몽타주 |

### 함수

| 함수명 | 반환 타입 | 설명 |
|--------|----------|------|
| `GetWeaponTag()` | `FGameplayTag` | 무기 태그 반환 |
| `GetSocketName()` | `FName` | 부착 소켓 이름 반환 |
| `GetWeaponMesh()` | `USkeletalMeshComponent*` | 무기 메시 컴포넌트 반환 |
| `GetEquipAnimLayer()` | `TSubclassOf<UAnimInstance>` | 애니메이션 레이어 클래스 반환 |
| `GetEquipMontage()` | `UAnimMontage*` | 장착 몽타주 반환 |
| `GetUnequipMontage()` | `UAnimMontage*` | 해제 몽타주 반환 |

### 생성자 동작
- `PrimaryActorTick.bCanEverTick = false`
- `WeaponMesh` 생성 및 루트 컴포넌트로 설정
- 충돌 비활성화

---

## 2. ATimeThiefRifle

**파일 위치:** `Source/TimeThief/Weapon/TimeThiefRifle.h/cpp`

**역할:** 히트스캔 방식의 자동 소총 구현. 발사, 재장전, 반동, 데미지 적용을 처리합니다.

### 구조체: FHitScanResult

히트스캔 결과를 저장하는 구조체입니다.

| 변수명 | 타입 | 설명 |
|--------|------|------|
| `bHit` | `bool` | 명중 여부 |
| `HitLocation` | `FVector` | 충돌 위치 |
| `HitNormal` | `FVector` | 충돌 표면 법선 벡터 |
| `HitActor` | `TWeakObjectPtr<AActor>` | 맞은 액터 (약참조) |
| `HitBoneName` | `FName` | 맞은 본 이름 |
| `FireDirection` | `FVector` | 실제 발사 방향 |

### 변수

#### 스탯 관련

| 변수명 | 타입 | 기본값 | 설명 |
|--------|------|--------|------|
| `BaseDamage` | `float` | 25.0f | 기본 데미지 |
| `HeadshotMultiplier` | `float` | 2.0f | 헤드샷 데미지 배율 |
| `FireRate` | `float` | 600.0f | 분당 발사 속도 (RPM) |
| `MaxRange` | `float` | 10000.0f | 최대 사거리 (cm) |
| `SpreadAngle` | `float` | 1.0f | 기본 탄퍼짐 각도 (도) |
| `HeadshotBoneName` | `FName` | "head" | 헤드샷 판정용 본 이름 |

#### 탄약 관련

| 변수명 | 타입 | 기본값 | 설명 |
|--------|------|--------|------|
| `MaxAmmo` | `int32` | 30 | 탄창 최대 탄약 |
| `MaxReserveAmmo` | `int32` | 120 | 예비 탄약 최대치 |
| `ReloadTime` | `float` | 2.0f | 재장전 시간 (초) |
| `CurrentAmmo` | `int32` | - | 현재 탄창 탄약 (런타임) |
| `ReserveAmmo` | `int32` | - | 현재 예비 탄약 (런타임) |

#### 이펙트 관련

| 변수명 | 타입 | 설명 |
|--------|------|------|
| `MuzzleFlashEffect` | `TObjectPtr<UParticleSystem>` | 총구 화염 이펙트 |
| `ImpactEffect` | `TObjectPtr<UParticleSystem>` | 탄착 이펙트 |
| `FireSound` | `TObjectPtr<USoundCue>` | 발사 사운드 |
| `ReloadSound` | `TObjectPtr<USoundCue>` | 재장전 사운드 |
| `MuzzleSocketName` | `FName` | 총구 소켓 이름 |

#### 애니메이션 관련

| 변수명 | 타입 | 설명 |
|--------|------|------|
| `FireMontage` | `TObjectPtr<UAnimMontage>` | 발사 애니메이션 몽타주 |
| `ReloadMontage` | `TObjectPtr<UAnimMontage>` | 재장전 애니메이션 몽타주 |

#### 상태 변수 (Private)

| 변수명 | 타입 | 설명 |
|--------|------|------|
| `bIsFiring` | `bool` | 발사 중 여부 |
| `bIsReloading` | `bool` | 재장전 중 여부 |
| `AutoFireTimerHandle` | `FTimerHandle` | 자동 발사 타이머 핸들 |
| `ReloadTimerHandle` | `FTimerHandle` | 재장전 타이머 핸들 |

### 함수

#### Public 함수

| 함수명 | 반환 타입 | 설명 |
|--------|----------|------|
| `StartFire()` | `void` | 발사 시작 (자동 발사 루프 시작) |
| `StopFire()` | `void` | 발사 중지 |
| `Reload()` | `void` | 재장전 시작 |
| `CanFire()` | `bool` | 발사 가능 여부 반환 |
| `GetCurrentAmmo()` | `int32` | 현재 탄약 반환 |
| `GetMaxAmmo()` | `int32` | 최대 탄창 용량 반환 |
| `GetReserveAmmo()` | `int32` | 예비 탄약 반환 |
| `IsReloading()` | `bool` | 재장전 중 여부 |
| `IsFiring()` | `bool` | 발사 중 여부 |

#### Protected 함수

| 함수명 | 반환 타입 | 설명 |
|--------|----------|------|
| `BeginPlay()` | `void` | 시작 시 탄약 초기화 |
| `EndPlay()` | `void` | 종료 시 타이머 정리 |
| `Tick()` | `void` | 자동 발사 처리 |
| `FireShot()` | `void` | 단발 발사 실행 |
| `PerformHitScan()` | `FHitScanResult` | 히트스캔 수행 및 결과 반환 |
| `ApplyDamage()` | `void` | 타격 대상에 데미지 적용 |
| `PlayFireEffects()` | `void` | 발사 이펙트/사운드 재생 |
| `PlayImpactEffects()` | `void` | 탄착 이펙트 재생 |
| `FinishReload()` | `void` | 재장전 완료 처리 |
| `GetMuzzleLocation()` | `FVector` | 총구 위치 반환 |
| `GetAimDirection()` | `FVector` | 조준 방향 반환 |
| `ApplyRecoil()` | `void` | 반동 적용 |

### 발사 로직 플로우 (Timer 기반)

```
StartFire()
    │
    ├── bIsReloading 또는 bIsFiring 체크
    │
    ├── bIsFiring = true
    │
    ├── CanFire() 체크
    │       │
    │       └── 통과 시 FireShot() 즉시 1회 실행
    │
    └── FTimerManager::SetTimer()
            │
            └── FireInterval(60/FireRate) 간격으로 FireShot() 반복 호출

FireShot()
    │
    ├── CanFire() 체크
    │       │
    │       └── 실패 시 StopFire() 호출 및 자동 Reload()
    │
    ├── CurrentAmmo--
    │
    ├── PerformHitScan()
    │       │
    │       ├── 총구 위치 & 조준 방향 계산
    │       ├── 탄퍼짐 적용 (VRandCone)
    │       └── LineTrace 실행
    │
    ├── ApplyDamage() (명중 시)
    │       │
    │       ├── 헤드샷 판정
    │       └── UGameplayStatics::ApplyPointDamage() 호출
    │
    ├── PlayImpactEffects() (명중 시)
    │
    ├── PlayFireEffects()
    │
    └── ApplyRecoil()

StopFire()
    │
    ├── bIsFiring = false
    │
    └── FTimerManager::ClearTimer(AutoFireTimerHandle)
```

---

## 3. UTimeThiefHealthComponent

**파일 위치:** `Source/TimeThief/Components/TimeThiefHealthComponent.h/cpp`

**역할:** 캐릭터/액터의 체력을 관리하고 데미지/힐 처리 및 사망 이벤트를 담당합니다.

### 델리게이트

| 델리게이트 | 파라미터 | 설명 |
|-----------|---------|------|
| `FOnHealthChangedSignature` | HealthComponent, OldHealth, NewHealth, Instigator | 체력 변경 시 브로드캐스트 |
| `FOnDeathSignature` | OwningActor | 사망 시 브로드캐스트 |

### 변수

| 변수명 | 타입 | 기본값 | 설명 |
|--------|------|--------|------|
| `DefaultMaxHealth` | `float` | 100.0f | 기본 최대 체력 |
| `CurrentHealth` | `float` | - | 현재 체력 |
| `MaxHealth` | `float` | - | 최대 체력 |
| `bIsDead` | `bool` | false | 사망 여부 |
| `LastDamageInstigator` | `TObjectPtr<AActor>` | - | 마지막 데미지 가해자 |

### 함수

| 함수명 | 반환 타입 | 설명 |
|--------|----------|------|
| `InitializeWithHealth()` | `void` | 지정된 최대 체력으로 초기화 |
| `GetCurrentHealth()` | `float` | 현재 체력 반환 |
| `GetMaxHealth()` | `float` | 최대 체력 반환 |
| `GetHealthPercent()` | `float` | 체력 비율 (0.0~1.0) 반환 |
| `IsDead()` | `bool` | 사망 여부 반환 |
| `TakeDamage()` | `void` | 데미지 적용 |
| `Heal()` | `void` | 힐 적용 |
| `HandleDeath()` | `void` | 사망 처리 (내부) |
| `OnTakeAnyDamageCallback()` | `void` | 언리얼 표준 데미지 수신 콜백 |

### 데미지 플로우

```
[외부에서 UGameplayStatics::ApplyPointDamage() 호출]
    │
    └── Owner Actor의 OnTakeAnyDamage 델리게이트 트리거
            │
            └── OnTakeAnyDamageCallback()
                    │
                    └── TakeDamage() 호출

TakeDamage(DamageAmount, DamageInstigator)
    │
    ├── bIsDead 또는 DamageAmount <= 0 체크
    │
    ├── CurrentHealth -= DamageAmount (클램핑)
    │
    ├── LastDamageInstigator 저장
    │
    ├── OnHealthChanged.Broadcast()
    │
    └── CurrentHealth <= 0 → HandleDeath()
                                    │
                                    ├── bIsDead = true
                                    │
                                    └── OnDeath.Broadcast()
```

---

## 4. UTimeThiefPawnCombatComponent

**파일 위치:** `Source/TimeThief/Components/Combat/TimeThiefPawnCombatComponent.h/cpp`

**역할:** Pawn의 무기 인벤토리 관리, 장착/해제, 입력 처리의 베이스 클래스입니다.

### 열거형: EToggleDamageType

| 값 | 설명 |
|----|------|
| `CurrentEquippedWeapon` | 현재 장착 무기 |
| `LeftHand` | 왼손 |
| `RightHand` | 오른손 |

### 변수

| 변수명 | 타입 | 설명 |
|--------|------|------|
| `CurrentEquippedWeaponTag` | `FGameplayTag` | 현재 장착된 무기 태그 |
| `CharacterCarriedWeaponMap` | `TMap<FGameplayTag, TObjectPtr<ATimeThiefWeaponBase>>` | 소지 무기 맵 |
| `CurrentEquippedWeapon` | `TObjectPtr<ATimeThiefWeaponBase>` | 현재 장착 무기 포인터 |

### 함수

| 함수명 | 반환 타입 | 설명 |
|--------|----------|------|
| `RegisterSpawnedWeapon()` | `void` | 생성된 무기를 인벤토리에 등록 |
| `GetCharacterCarriedWeaponByTag()` | `ATimeThiefWeaponBase*` | 태그로 소지 무기 검색 |
| `GetCharacterCurrentEquippedWeapon()` | `ATimeThiefWeaponBase*` | 현재 장착 무기 반환 |
| `EquipWeapon()` | `void` | 무기 장착 |
| `UnequipCurrentWeapon()` | `void` | 현재 무기 해제 |
| `HandleInputPressed()` | `void` | 입력 눌림 처리 (가상) |
| `HandleInputReleased()` | `void` | 입력 해제 처리 (가상) |
| `AttachWeaponToSocket()` | `void` | 무기를 캐릭터 소켓에 부착 |
| `PlayEquipMontage()` | `void` | 장착 몽타주 재생 |

### 무기 장착 플로우

```
EquipWeapon(WeaponTag)
    │
    ├── GetCharacterCarriedWeaponByTag()로 무기 검색
    │
    ├── 이미 장착된 무기면 리턴
    │
    ├── 기존 무기 있으면 UnequipCurrentWeapon()
    │
    ├── CurrentEquippedWeaponTag & CurrentEquippedWeapon 설정
    │
    ├── SetActorHiddenInGame(false)
    │
    ├── AttachWeaponToSocket()
    │       │
    │       ├── 1인칭 모드: FirstPersonMesh에 부착
    │       └── 3인칭 모드: Character Mesh에 부착
    │
    ├── LinkAnimClassLayers() (애니메이션 레이어 링크)
    │
    └── PlayEquipMontage()
```

---

## 5. UTimeThiefPlayerCombatComponent

**파일 위치:** `Source/TimeThief/Components/Combat/TimeThiefPlayerCombatComponent.h/cpp`

**역할:** 플레이어 전용 전투 컴포넌트. 기본 무기 스폰 및 입력 핸들링을 담당합니다.

### 변수

| 변수명 | 타입 | 설명 |
|--------|------|------|
| `DefaultWeaponClasses` | `TArray<TSubclassOf<ATimeThiefWeaponBase>>` | 시작 시 자동 스폰할 무기 클래스 배열 |

### 함수

| 함수명 | 반환 타입 | 설명 |
|--------|----------|------|
| `BeginPlay()` | `void` | 기본 무기들 스폰 |
| `SpawnAndRegisterWeapon()` | `ATimeThiefWeaponBase*` | 무기 스폰 및 등록 |
| `HandleInputPressed()` | `void` | 입력 처리 (발사, 재장전, 장착) |
| `HandleInputReleased()` | `void` | 입력 해제 처리 (발사 중지) |

### 입력 처리 플로우

```
HandleInputPressed(InputTag)
    │
    ├── InputTag_Action_EquipRifle
    │       ├── 이미 장착됨 → UnequipCurrentWeapon()
    │       └── 미장착 → EquipWeapon(Weapon_Rifle)
    │
    ├── InputTag_Action_Fire
    │       └── ATimeThiefRifle::StartFire()
    │
    └── InputTag_Action_Reload
            └── ATimeThiefRifle::Reload()

HandleInputReleased(InputTag)
    │
    └── InputTag_Action_Fire
            └── ATimeThiefRifle::StopFire()
```

---

## GameplayTags

총기 시스템에서 사용하는 태그들:

### 입력 태그

| 태그 | 설명 |
|------|------|
| `InputTag.Action.Fire` | 발사 입력 |
| `InputTag.Action.Reload` | 재장전 입력 |
| `InputTag.Action.EquipRifle` | 라이플 장착/해제 입력 |

### 무기 태그

| 태그 | 설명 |
|------|------|
| `Weapon.Rifle` | 라이플 무기 타입 |
| `Weapon.Pistol` | 권총 무기 타입 |

### 상태 태그

| 태그 | 설명 |
|------|------|
| `State.Combat.Rifle` | 라이플 장착 상태 |
| `State.Combat.Pistol` | 권총 장착 상태 |

---

## 클래스 의존성 다이어그램

```
ATimeThiefRifle ────┬───> UTimeThiefHealthComponent (데미지 적용)
                    │
                    └───> UTimeThiefPlayerAnimInstance (반동/탄퍼짐)

UTimeThiefPlayerCombatComponent ────> ATimeThiefRifle (발사/재장전 호출)
                    │
                    └───> ATimeThiefWeaponBase (장착/해제)

ATimeThiefPlayerCharacter ────> UTimeThiefPlayerCombatComponent
                    │
                    └───> UTimeThiefHealthComponent
```

---

## 확장 가이드

### 새로운 무기 추가

1. `ATimeThiefWeaponBase`를 상속하는 새 클래스 생성
2. 필요한 발사 로직 구현
3. `TimeThiefGameplayTags`에 새 무기 태그 추가
4. `UTimeThiefPlayerCombatComponent::HandleInputPressed()`에 새 무기 입력 처리 추가

### 데미지 타입 확장

1. `UDamageType` 상속 클래스 생성
2. `ATimeThiefRifle::ApplyDamage()`에서 `UGameplayStatics::ApplyDamage()` 사용으로 변경
3. `UTimeThiefHealthComponent`에서 데미지 타입별 처리 추가

