

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
	
	// UFUNCTION(Exec)
	// void JoinRoom();
	//
	// UFUNCTION(Exec)
	// void LeaveRoom();
	
// Testing 용
public:
	UFUNCTION(Exec)
	void Pos();
	
	UFUNCTION(Exec)
	void Tp(float X, float Y, float Z);

	UFUNCTION(Exec)
	void TpAll(float X, float Y, float Z);
	
	UFUNCTION(Exec)
	void TestSpawnMonster(float X, float Y, float Z, int32 MonsterType);
	
	UFUNCTION(Exec)
	void TestSpawnChest(float X, float Y, float Z);
	
	UFUNCTION(Exec)
	void TestSpawnStore(float X, float Y, float Z);
	
	UFUNCTION(Exec)
	void TestItemReq(int32 ItemId, int32 Amount);
	
	UFUNCTION(Exec)
	void TestMoneyReq(int32 Amount);
	
	UFUNCTION(Exec)
	void TestHealthReq(int32 Health);
	
	UFUNCTION(Exec)
	void TestMaxHealthReq(int32 MaxHealth);
	
	UFUNCTION(Exec)
	void TestZoneStop();
	
	UFUNCTION(Exec)
	void TestZoneStart();
	
	UFUNCTION(Exec)
	void TestZoneReset();
	
	UFUNCTION(Exec)
	void TestZoneDamageOff();
	
	UFUNCTION(Exec)
	void TestZoneDamageOn();
	
};
