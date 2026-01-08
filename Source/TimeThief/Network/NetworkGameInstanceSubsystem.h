// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Sockets.h"
#include "TimerManager.h"
#include "Engine/World.h"

#include "SendBuffer.h"

#include "NetworkGameInstanceSubsystem.generated.h"

class SendBuffer;;

/*---------------------------------
   NetworkGameInstanceSubsystem
---------------------------------*/
//
// NetworkGameInstanceSubsystem는 네트워크 기능을 담당하는 게임 인스턴스 서브시스템 클래스입니다.
//

UCLASS()
class TIMETHIEF_API UNetworkGameInstanceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
public:
	UFUNCTION(BLueprintCallable, Category = "Network", meta = (WorldContext="WorldContextObject"))
	static UNetworkGameInstanceSubsystem* Get(UObject* WorldContextObject);
	
public:
	UFUNCTION(BLueprintCallable, Category = "Network")
	void SendPacket(const SendBuffer& Buffer);
	
private:
	void COnnectToServer(const FString& IPAddress, int32 Port);
	
	void SpawnProcessMessagesTimer();
	
	void Send(const class SendBuffer& Buffer);
	
	void ProcessPacket();
	
private:
	bool bIsConnected = false;
	FSocket* Socket = nullptr;
	
	FString ServerAddress = TEXT("127.0.0.1");	// 기본값 localhost(loopback)
	int ServerPort = 8252;						// TimeThiefServer 포트
	
	// TODO: ClientSession을 먼저 작성하기
	// TSharedPtr<class ClientSession> ClientSession;
	
	FTimerHandle QueueProcessingTimer;
	
};
