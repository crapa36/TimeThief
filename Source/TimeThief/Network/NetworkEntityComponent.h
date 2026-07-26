#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "Network/State/NetworkControlType.h"

#include "NetworkEntityComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnNetworkControlTypeChanged, ENetworkControlType);

UCLASS(ClassGroup=(Network), meta=(BlueprintSpawnableComponent))
class TIMETHIEF_API UNetworkEntityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNetworkEntityComponent();
	
	uint32 GetEntityId() const { return EntityId; }
	void SetEntityId(uint32 InEntityId) { EntityId = InEntityId; }

	ENetworkControlType GetControlType() const { return ControlType; }
	void SetControlType(ENetworkControlType InControlType);
	FOnNetworkControlTypeChanged OnControlTypeChanged;
	
	bool IsValidEntity() const { return EntityId != 0; }
	
	bool IsLocalControlled() const { return ControlType == ENetworkControlType::Local; }
	bool IsRemoteControlled() const { return ControlType == ENetworkControlType::Remote; }
	bool IsServerAuthoritative() const { return ControlType == ENetworkControlType::ServerAuth; }
	
	bool CanSendInput() const;
	bool ShouldApplyNetworkState() const;
	
	void ResetNetworkEntity();
	
private:
	UPROPERTY()
	uint32 EntityId = 0;
	
	UPROPERTY()
	ENetworkControlType ControlType = ENetworkControlType::None;

};
