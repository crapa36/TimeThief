#pragma once

#include "CoreMinimal.h"
#include "MonsterVisualState.h"
#include "GameFramework/Pawn.h"
#include "Network/CombatSyncInterface.h"
#include "Network/MovableNetworkEntityInterface.h"
#include "Network/NetworkEntityInterface.h"
#include "Components/Combat/TimeThiefMonsterCombatComponent.h"
#include "Components/CapsuleComponent.h"
#include "Network/State/RemoteAttackNotify.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "Components/FX/TimeThiefDissolveFXComponent.h"

#include "TimeThiefMonster.generated.h"

class UNetworkMoveComponent;

UCLASS()
class TIMETHIEF_API ATimeThiefMonster : public APawn
	, public INetworkEntityInterface
	, public IMovableNetworkEntityInterface
	, public ICombatSyncInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	ATimeThiefMonster();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
		
	// Network Entity
public:
	virtual UNetworkEntityComponent* GetNetworkEntityComponent() const override;
	
	// MovableNetworkEntity
public:
	virtual FVector GetNetworkLocation() const override;
	virtual void SetNetworkLocation(const FVector& NewLocation) override;
	
	virtual float GetNetworkCharYaw() const override;
	virtual void SetNetworkCharYaw(float NewCharYaw) override;
	
	virtual float GetNetworkAimYaw() const override;
	virtual void SetNetworkAimYaw(float NewAimYaw) override;
	
	virtual float GetNetworkAimPitch() const override;
	virtual void SetNetworkAimPitch(float NewAimPitch) override;
	
	virtual FVector2D GetNetworkVelocity2D() const override;
	virtual void SetNetworkVelocity2D(FVector2D NewVelocity) override;
	
	virtual EMovementMode GetNetworkMovementMode() const override;
	virtual void SetNetworkMovementMode(EMovementMode NewMovementMode) override;
	
	virtual float GetLocalControlAimYaw() const override;
	virtual float GetLocalControlAimPitch() const override;
	virtual FVector2D GetLocalControlVelocity2D() const override;
	virtual EMovementMode GetLocalControlMovementMode() const override;
	
	virtual FVector GetMoveStep() const override;
	virtual void ApplyNetworkMovementState(const FNetworkEntityState& EntityState) override;
	
	UFUNCTION(BlueprintCallable, Category = "Network")
	float GetNetworkSpeed() const { return GetMoveStep().Size2D(); }
	
// Anim Instance
public:
	UFUNCTION(BlueprintCallable, BlueprintPure)
	float GetAimYaw() const;
	
	UFUNCTION(BlueprintCallable, BlueprintPure)
	float GetAimPitch() const;
	
	UFUNCTION(BlueprintCallable, BlueprintPure)
	bool IsDead() const;
	
	// CombatSyncInterface
public:
	virtual class UTimeThiefPawnCombatComponent* GetCombatComponent() const override;
	virtual class UNetworkCombatSyncComponent* GetCombatSyncComponent() const override;
	virtual uint32 GetCombatEntityId() const override;
	
public:
	uint32 GetEntityId() const;
	
	UFUNCTION(BlueprintCallable, Category = "Network")
	UNetworkMoveComponent* GetNetworkMoveComponent() const { return NetworkMoveComponent; }
	
	UFUNCTION(BlueprintCallable, Category = "Network")
	UNetworkCombatSyncComponent* GetNetworkCombatSyncComponent() const { return NetworkCombatSyncComponent; }
	
public:
	void SetTarget(uint32 InTargetId, AActor* InTargetActor);
	
	UFUNCTION(BlueprintCallable, Category="Combat")
	void HandleRemoteCombatRequest(const FRemoteAttackNotify& AttackRequest);

	UFUNCTION(BlueprintCallable, Category="Combat")
	void OnDeathNetwork();

	UFUNCTION(BlueprintCallable, Category="Combat")
	void OnRespawnNetwork(const FVector& SpawnLocation, const FRotator& SpawnRotation);
	
	void RemoteCombat(const FRemoteAttackNotify& AttackNotify);
	
	void RemoteFire(const FRemoteAttackNotify& AttackNotify);
	void RemoteAttack(const FRemoteAttackNotify& AttackNotify);
	
	void RemoteHit(const FRemoteAttackNotify& AttackNotify);
	void RemoteCancelAttack(const FRemoteAttackNotify& AttackNotify);
	
	UAnimMontage* GetAttackMontage(int32 AttackType) const;

	void StartDeathDisappearEffect();
	void FinishDeathHide();
	void PlayRespawnEffect();
	void PlayRewardBurstFX();
	void FinishRespawn();
	void OnDeathMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void FreezeDeathPoseAndStartDisappearEffect();
	
	void DisableCombatCollision();
	void EnableCombatCollision();
	void StopMovementVisual();
	
	void OnDeathMontageFinishedFallback();
	
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Monster", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> SceneRootComponent = nullptr;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Monster", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCapsuleComponent> CapsuleComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Monster", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USkeletalMeshComponent> MeshComponent = nullptr;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Network", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UNetworkEntityComponent> NetworkEntityComponent = nullptr;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Network", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UNetworkMoveComponent> NetworkMoveComponent = nullptr;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Network", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UNetworkCombatSyncComponent> NetworkCombatSyncComponent = nullptr;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UTimeThiefMonsterCombatComponent> MonsterCombatComponent = nullptr;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Network")
	FVector2D CurrentNetworkVelocity = FVector2D::ZeroVector;
	
// BP에서 설정 필수 항목 
// -----------------------------------------------------------------------------------	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Animation")
	TMap<int32, TObjectPtr<UAnimMontage>> AttackMontageMap;
	// AttackType(=AttackId)에 따른 몽타주 매핑
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Animation")
	TObjectPtr<UAnimMontage> HitReactMontage = nullptr;
	// 피격 리액션 몽타주

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Animation")
	TObjectPtr<UAnimMontage> DeathMontage = nullptr;
	// 사망 몽타주
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Animation")
	TObjectPtr<UAnimMontage> RespawnMontage = nullptr;
	// 부활 몽타주

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Animation")
	float DeathPoseFreezeLeadTime = 0.25f;
	// 사망 몽타주 종료 직전에 포즈를 고정하기 위한 여유 시간
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|VFX")
	TObjectPtr<UNiagaraSystem> FireCastFX = nullptr;
	// 사격 시전 이펙트 (총알 궤적 이펙트)
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|VFX")
	TObjectPtr<UNiagaraSystem> FireImpactFX = nullptr;
	// 사격 폭발 이펙트 (총구의 폭발 이펙트)

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Reward|VFX")
	TObjectPtr<UNiagaraSystem> RewardBurstFX = nullptr;
	// 보상 획득이 발생하는 사망 순간에 한 번 재생되는 이펙트

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Reward|VFX")
	FVector RewardBurstFXOffset = FVector::ZeroVector;
	// Actor 기준 RewardBurstFX 위치 보정

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Reward|VFX")
	FVector RewardBurstFXScale = FVector{2.0f, 2.0f, 2.0f};
	// Actor별 RewardBurstFX 크기 보정
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Sound")
	TObjectPtr<USoundBase> FireSound = nullptr;
	// 사격 사운드
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Sound")
	TObjectPtr<USoundAttenuation> MonsterSoundAttenuation = nullptr;
	// 지역 사운드 감쇠 설정
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat|VFX", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UTimeThiefDissolveFXComponent> DissolveFXComponent = nullptr;
	// 디졸브 이펙트 컴포넌트 (사망과 부활 시 디졸브 효과를 담당하는 컴포넌트)
// -----------------------------------------------------------------------------------	
	
	UPROPERTY()
	TObjectPtr<UAnimMontage> CurrentAttackMontage = nullptr;
	
	UPROPERTY(BlueprintReadOnly)
	bool bIsDead = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Reward|VFX")
	bool bRewardBurstFXPlayed = false;
	
	UPROPERTY(BlueprintReadOnly)
	EMonsterVisualState VisualState = EMonsterVisualState::Alive;
	
	FTimerHandle DeathHideTimerHandle;
	FTimerHandle RespawnFinishTimerHandle;
	
	uint32 TargetId = 0;
	TWeakObjectPtr<AActor> TargetActor;
	
};
