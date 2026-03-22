#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"

#include "Network/MovableNetworkEntityInterface.h"
#include "Network/State/NetworkControlType.h"
#include "Network/NetworkEntityInterface.h"

#include "NTPlayer.generated.h"

class UNetworkMoveComponent;
class UNetworkEntityComponent;
struct FNetworkEntityState;

UCLASS()
class TIMETHIEF_API ANTPlayer : public ACharacter, public INetworkEntityInterface, public IMovableNetworkEntityInterface
// Network Test Player
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ANTPlayer();
	virtual ~ANTPlayer();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

public:
	virtual FVector GetNetworkLocation() const override;
	virtual void SetNetworkLocation(const FVector& NewLocation) override;
	
	virtual float GetNetworkYaw() const override;
	virtual void SetNetworkYaw(float NewYaw) override;
	
	virtual float GetNetworkPitch() const override;
	virtual void SetNetworkPitch(float NewPitch) override;
	
	virtual float GetNetworkSpeed() const override;
	virtual void SetNetworkSpeed(float NewSpeed) override;
	
	virtual float GetLocalControlPitch() const override;
	virtual float GetLocalControlSpeed() const override;
	
	virtual FVector GetNetworkVelocity() const override;
	virtual void ApplyNetworkMovementState(const FNetworkEntityState& EntityState) override;
	
public:
	virtual UNetworkEntityComponent* GetNetworkEntityComponent() const override;
	
public:
	bool IsLocalPlayer() const;
	uint32 GetEntityId() const;
	
public:
	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	
private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Network", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UNetworkEntityComponent> NetworkEntityComponent = nullptr;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Network", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UNetworkMoveComponent> NetworkMoveComponent = nullptr;
	
protected:
	UPROPERTY(VisibleAnywhere, Category = "Network")
	float CurrentNetworkPitch = 0.0f;
	
	UPROPERTY(VisibleAnywhere, Category = "Network")
	float CurrentNetworkSpeed = 0.0f;
	
};
