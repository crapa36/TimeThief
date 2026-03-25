#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "NetworkEntityInterface.generated.h"

struct FNetworkEntityState;

// This class does not need to be modified.
UINTERFACE(Blueprintable)
class UNetworkEntityInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class TIMETHIEF_API INetworkEntityInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	// virtual ~INetworkEntityInterface() = default;
	
public:
	// virtual uint32 GetEntityId() const = 0;
	// virtual void SetEntityId(uint32 NewEntityId) = 0;
	//
	// virtual bool IsLocalPlayer() const = 0;
	// virtual void SetIsLocalEntity(bool bIsLocal) = 0;
	//
	// virtual void InitializeFromNetworkState(const FNetworkEntityState& EntityState) = 0;
	// virtual void ApplyNetworkState(const FNetworkEntityState& EntityState) = 0;
	// virtual void HandleNetworkDespawn() = 0;
	
	virtual class UNetworkEntityComponent* GetNetworkEntityComponent() const = 0;
	
};
