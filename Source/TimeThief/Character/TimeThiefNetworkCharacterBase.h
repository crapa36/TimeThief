#pragma once

#include "CoreMinimal.h"

#include "TimeThiefCharacterBase.h"

#include "Network/CombatSyncInterface.h"
#include "Network/MovableNetworkEntityInterface.h"
#include "Network/NetworkEntityInterface.h"

#include "TimeThiefNetworkCharacterBase.generated.h"

class UNetworkMoveComponent;

UCLASS()
class TIMETHIEF_API ATimeThiefNetworkCharacterBase 
	: public ATimeThiefCharacterBase
	, public INetworkEntityInterface
	, public IMovableNetworkEntityInterface
	, public ICombatSyncInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ATimeThiefNetworkCharacterBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual ~ATimeThiefNetworkCharacterBase();

public:
	virtual FRotator GetBaseAimRotation() const override;

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

// CombatSyncInterface
public:
	virtual class UTimeThiefPawnCombatComponent* GetCombatComponent() const override;
	virtual class UNetworkCombatSyncComponent* GetCombatSyncComponent() const override;
	virtual uint32 GetCombatEntityId() const override;
	
// 공통 유틸
public:
	bool IsLocalPlayer() const;
	uint32 GetEntityId() const;
	
	UFUNCTION(BlueprintCallable, Category = "Network")
	UNetworkMoveComponent* GetNetworkMoveComponent() const { return NetworkMoveComponent; }
	
	UFUNCTION(BlueprintCallable, Category = "Network")
	UNetworkCombatSyncComponent* GetNetworkCombatSyncComponent() const { return NetworkCombatSyncComponent; }
	
	bool bIsJumping = false;
	
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Network", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UNetworkEntityComponent> NetworkEntityComponent = nullptr;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Network", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UNetworkMoveComponent> NetworkMoveComponent = nullptr;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Network", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UNetworkCombatSyncComponent> NetworkCombatSyncComponent = nullptr;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Network")
	float CurrentNetworkAimYaw = 0.0f;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Network")
	float CurrentNetworkAimPitch = 0.0f;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Network")
	float CurrentNetworkSpeed = 0.0f;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Network")
	FVector2D CurrentNetworkVelocity = FVector2D::ZeroVector;
	
	EMovementMode CurrentNetworkMovementMode = EMovementMode::MOVE_None;
	
};
