#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "Network/State/NetworkControlType.h"

#include "NetworkEntityComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class TIMETHIEF_API UNetworkEntityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	uint32 GetEntityId() const { return EntityId; }
	void SetEntityId(uint32 InEntityId) { EntityId = InEntityId; }

	ENetworkControlType GetControlType() const { return ControlType; }
	void SetControlType(ENetworkControlType InControlType) { ControlType = InControlType; }
	
	bool IsLocalControlled() const { return ControlType == ENetworkControlType::Local; }
	bool IsRemoteControlled() const { return ControlType == ENetworkControlType::Remote; }
	bool IsServerAuthoritative() const { return ControlType == ENetworkControlType::ServerAuth; }
	
	bool CanSendInput() const { return ControlType == ENetworkControlType::Local; }
	bool ShouldApplyNetworkState() const { return ControlType != ENetworkControlType::Local; }
	
private:
	UPROPERTY()
	uint32 EntityId = 0;
	
	UPROPERTY()
	ENetworkControlType ControlType = ENetworkControlType::None;

};
