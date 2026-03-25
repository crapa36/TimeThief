

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "NTCheatManager.generated.h"

/**
 * 
 */
UCLASS()
class TIMETHIEF_API UNTCheatManager : public UCheatManager
{
	GENERATED_BODY()
	
public:
	UFUNCTION(Exec)
	void JoinRoom();
	
	UFUNCTION(Exec)
	void LeaveRoom();
};
