# TimeThief 클래스 참조 문서

## 1. Core System

### TimeThief (Module)
게임의 메인 모듈. 게임 시작 시 Native GameplayTags를 초기화.

### TimeThiefAssetManager
UAssetManager 상속. PrimaryDataAsset들(PawnData, InputConfig, ExperienceDefinition)의 로딩과 관리를 담당.

### TimeThiefGameplayTags (Singleton)
네이티브 GameplayTag들을 정의하고 관리. Input, Weapon, State, InitState 카테고리의 태그들을 등록하며, `FTimeThiefGameplayTags::Get()`으로 전역 접근.

---

## 2. Character

### ATimeThiefCharacterBase
모든 캐릭터의 기본 클래스. ACharacter 상속. 기본적인 컴포넌트 설정만 포함.

### ATimeThiefPlayerCharacter
플레이어 캐릭터. 카메라(SpringArm + Camera), HeroComponent, PlayerCombatComponent, CharacterTrajectoryComponent를 생성. PawnData를 받으면 HeroComponent에 전달하여 입력 초기화 시작.

### ATimeThiefEnemyCharacter
적 캐릭터 기본 클래스. EnemyCombatComponent 포함 예정.

### UTimeThiefPawnData (DataAsset)
Pawn 설정 데이터. InputConfig 참조를 포함하여 캐릭터가 사용할 입력 설정을 정의.

### ATimeThiefPlayerController
플레이어 컨트롤러. Enhanced Input 설정 (InputMappingContext 추가)을 담당.

---

## 3. Components

### UTimeThiefPawnExtensionComponent
IGameFrameworkInitStateInterface 구현. InitState 패턴을 통한 컴포넌트 초기화 순서 관리. 다른 컴포넌트들의 기본 클래스.

### UTimeThiefHeroComponent
플레이어 전용 컴포넌트. 입력 바인딩과 라우팅 담당:
- PawnData로부터 InputConfig를 받아 입력 바인딩
- Move/Look 입력은 직접 캐릭터 이동/회전 처리
- 전투 관련 입력은 CombatComponent.HandleInputPressed()로 라우팅

### UTimeThiefPawnCombatComponent
전투 시스템의 기본 컴포넌트:
- 무기 등록/관리 (CharacterCarriedWeaponMap)
- 무기 장착/해제 (EquipWeapon/UnequipCurrentWeapon)
- 입력 핸들링 가상 함수 (HandleInputPressed/Released)
- 무기 충돌 토글

### UTimeThiefPlayerCombatComponent
플레이어 전용 전투 컴포넌트. UTimeThiefPawnCombatComponent 상속:
- BeginPlay에서 DefaultWeaponClasses의 무기들을 자동 스폰
- SpawnAndRegisterWeapon() 함수로 무기 스폰 및 등록
- HandleInputPressed에서 EquipRifle 등 장비 관련 입력 처리

---

## 4. Input

### UTimeThiefInputComponent
UEnhancedInputComponent 상속. GameplayTag 기반 입력 바인딩 지원:
- BindNativeAction(): Move, Look 등 기본 입력
- BindAbilityActions(): 전투 입력 (Started/Completed 이벤트)

### UTimeThiefInputConfig (DataAsset)
입력 설정 데이터. InputAction과 GameplayTag의 매핑 정의:
- NativeInputActions: Move, Look 등 기본 입력
- AbilityInputActions: Fire, Reload, EquipRifle 등 전투 입력

---

## 5. Weapon

### ATimeThiefWeaponBase
기본 무기 액터 클래스:
- WeaponMesh (SkeletalMeshComponent)
- WeaponTag: 무기 식별용 GameplayTag
- SocketName: 캐릭터에 부착할 소켓 이름
- EquipAnimLayer: 장착 시 적용할 애니메이션 레이어

---

## 6. Animation

### UTimeThiefAnimInstance
기본 애니메이션 인스턴스.

### UTimeThiefPlayerAnimInstance
플레이어 전용. Motion Matching 연동.

### UTimeThiefMotionMatchingLayers
무기 상태별 애니메이션 레이어 인터페이스.

---

## 7. Game

### ATimeThiefGameMode
게임 모드:
- DefaultExperience에서 PawnData 로드
- HandleStartingNewPlayer에서 생성된 캐릭터에 PawnData 설정

### UTimeThiefExperienceDefinition (DataAsset)
게임 경험 정의:
- DefaultPawnData: 플레이어용 PawnData
- GameFeaturesToEnable: 활성화할 게임 피처
- ExperienceTags: 경험 식별 태그

---

## 데이터 흐름

### 초기화 흐름
```
GameMode.InitGame()
  → Experience에서 PawnData 로드
  → HandleStartingNewPlayer()
    → PlayerCharacter.SetPawnData()
      → HeroComponent.SetPawnData()
        → InitializePlayerInput()
          → InputComponent에 입력 바인딩
```

### 입력 → 전투 흐름
```
PlayerController (Enhanced Input)
  → HeroComponent.Input_AbilityInputTagPressed(InputTag)
    → CombatComponent.HandleInputPressed(InputTag)
      → EquipWeapon() / Fire() / Reload() 등 실행
```

### 무기 장착 흐름
```
PlayerCombatComponent.BeginPlay()
  → DefaultWeaponClasses 순회
    → SpawnAndRegisterWeapon()
      → World->SpawnActor()
      → RegisterSpawnedWeapon() (Map에 등록)

HandleInputPressed(EquipRifle)
  → EquipWeapon(Weapon.Rifle)
    → CharacterCarriedWeaponMap에서 찾기
    → AttachToComponent() (캐릭터 메시에 부착)
    → LinkAnimClassLayers() (애니메이션 레이어 적용)
```

---

## BP 체크리스트 (무기 장착 문제 해결)

### 1. BP_TimeThiefCharacter (Content/Player/)
**PlayerCombatComponent에서:**
- [ ] `Default Weapon Classes` 배열에 `BP_Rifle` 추가
  - Details 패널 → PlayerCombatComponent → Default Weapon Classes → + 클릭 → BP_Rifle 선택

### 2. BP_Rifle (ATimeThiefWeaponBase 상속 BP)
**필수 설정:**
- [ ] `Weapon Tag` = `Weapon.Rifle` (드롭다운에서 선택)
- [ ] `Socket Name` = 캐릭터 스켈레탈 메시의 소켓 이름 (예: `weapon_r`, `hand_r`)
- [ ] `Weapon Mesh` 컴포넌트에 실제 무기 Skeletal Mesh 설정

**선택 설정:**
- [ ] `Equip Anim Layer` = 무기별 애니메이션 레이어 클래스

### 3. 디버그 로그 확인
플레이 후 Output Log에서 다음 확인:
```
PlayerCombatComponent::BeginPlay - DefaultWeaponClasses count: X
SpawnAndRegisterWeapon: Spawned BP_Rifle_C with tag Weapon.Rifle
PlayerCombatComponent::BeginPlay - Total weapons registered: X
```

**로그가 없으면:** DefaultWeaponClasses가 비어있음
**tag가 빈 문자열이면:** BP_Rifle의 WeaponTag 미설정

---

## 디버깅 가이드

### 무기가 등록되지 않는 경우
1. `DefaultWeaponClasses count: 0` → BP에서 배열에 무기 추가 필요
2. `Spawned with tag (empty)` → BP_Rifle의 WeaponTag 설정 필요
3. `Failed to spawn weapon` → 무기 BP 클래스 문제

### 무기가 보이지 않는 경우
1. WeaponMesh에 메시가 설정되었는지 확인
2. SocketName이 캐릭터 스켈레톤의 실제 소켓과 일치하는지 확인
3. SetActorHiddenInGame(false)가 호출되는지 확인
