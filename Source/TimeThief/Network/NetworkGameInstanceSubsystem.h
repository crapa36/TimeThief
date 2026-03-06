// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Sockets.h"
#include "TimerManager.h"
#include "Engine/World.h"

#include "SendBuffer.h"
#include "PacketSession.h"
#include "ClientConfigTypes.h"

#include "NetworkGameInstanceSubsystem.generated.h"

class SendBuffer;
class PacketSession;

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
	UFUNCTION(BlueprintCallable, Category = "Network", meta = (WorldContext="WorldContextObject"))
	static UNetworkGameInstanceSubsystem* Get(UObject* WorldContextObject);
	
public:
	void SendPacket(TSharedPtr<SendBuffer> Buffer);
	
private:
	void ConnectToServer(const FString& IPAddress, int32 Port);
	
	void SpawnProcessPacketTimer();
	
	void ProcessPacket();
	
private:
	bool LoadClientConfig();
	
private:
	bool bIsConnected = false;
	FSocket* Socket = nullptr;
	
	TSharedPtr<PacketSession> GameSession;
	
	FTimerHandle QueueProcessingTimer;
	
	FClientConfig ClientConfig;
	
};
