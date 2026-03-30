

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
	void SetNickname(const FString& Nickname);
	
	UFUNCTION(Exec)
	void EnterMatchQueue();
	
	UFUNCTION(Exec)
	void CancelMatchQueue();
	
	UFUNCTION(Exec)
	void JoinRoom();
	
	UFUNCTION(Exec)
	void LeaveRoom();
};
