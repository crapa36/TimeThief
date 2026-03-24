#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CombatSyncInterface.generated.h"

// This class does not need to be modified.
UINTERFACE()
class UCombatSyncInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class TIMETHIEF_API ICombatSyncInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual class UTimeThiefPawnCombatComponent* GetCombatComponent() const = 0;
	virtual class UNetworkCombatSyncComponent* GetCombatSyncComponent() const = 0;
	virtual uint32 GetCombatEntityId() const = 0;
	
};
