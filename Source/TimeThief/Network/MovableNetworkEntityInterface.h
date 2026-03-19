#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MovableNetworkEntityInterface.generated.h"

struct FNetworkEntityState;

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UMovableNetworkEntityInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class TIMETHIEF_API IMovableNetworkEntityInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual FVector GetNetworkLocation() const = 0;
	virtual void SetNetworkLocation(const FVector& NewLocation) = 0;
	
	virtual float GetNetworkYaw() const = 0;
	virtual void SetNetworkYaw(float NewYaw) = 0;
	
	virtual float GetNetworkPitch() const = 0;
	virtual void SetNetworkPitch(float NewPitch) = 0;
	
	virtual float GetLocalControlPitch() const = 0;
	
	virtual void ApplyNetworkMovementState(const FNetworkEntityState& EntityState) = 0;
	
};
