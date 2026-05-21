#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Network/CombatSyncInterface.h"
#include "Network/MovableNetworkEntityInterface.h"
#include "Network/NetworkEntityInterface.h"
#include "Components/Combat/TimeThiefMonsterCombatComponent.h"
#include "Components/CapsuleComponent.h"
#include "Network/State/RemoteAttackNotify.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

#include "TimeThiefMonster.generated.h"

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
	UFUNCTION(BlueprintCallable, Category="Combat")
	void HandleRemoteCombatRequest(const FRemoteAttackNotify& AttackRequest);
	
	void RemoteCombat(const FRemoteAttackNotify& AttackNotify);
	
	void RemoteFire(const FRemoteAttackNotify& AttackNotify);
	void RemoteAttack(const FRemoteAttackNotify& AttackNotify);
	
	void RemoteHit(const FRemoteAttackNotify& AttackNotify);
	void RemoteCancelAttack(const FRemoteAttackNotify& AttackNotify);
	
	UAnimMontage* GetAttackMontage(int32 AttackType) const;
	
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
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|VFX")
	TObjectPtr<UNiagaraSystem> FireCastFX = nullptr;
	// 사격 시전 이펙트 (총알 궤적 이펙트)
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|VFX")
	TObjectPtr<UNiagaraSystem> FireImpactFX = nullptr;
	// 사격 폭발 이펙트 (총구의 폭발 이펙트)
// -----------------------------------------------------------------------------------	
	
	UPROPERTY()
	TObjectPtr<UAnimMontage> CurrentAttackMontage = nullptr;
	
};
