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
#include "Protocol.pb.h"
#include "Character/TimeThiefPawnData.h"
#include "DataAssets/SpawnClassData.h"
#include "State/LocalPlayerInfo.h"
#include "State/NetworkEntityState.h"
#include "State/NetworkPlayState.h"
#include "State/EntityRuntimeEntry.h"
#include "State/RoomState.h"
#include "State/RuntimeConfig.h"

#include "NetworkGameInstanceSubsystem.generated.h"

struct FRemoteAttackNotify;
struct FEntityRuntimeEntry;
struct FMoveSyncData;
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
	
public:
	void SendMove(const FMoveSyncData& MoveData);
	void SendJump();
	void SendJumpLand();
	
private:
	void ConnectToServer();
	void DisconnectFromServer();
	
	bool TryConnect(const FString& Host, int32 Port);
	void HandleConnectFailed();
	
	void Handshaking();
	
	void SpawnProcessPacketTimer();
	
	void ProcessPacket();
	
public:
	bool IsConnected() const;
	bool CanSendGameplayPacket() const;
	
// packet을 처리할 때 필요한 함수들 (예: 패킷 디스패치, 핸들러 등)
public:
	void HandleHandshakeRes(const se::auth::S_HandshakeRes& Pkt);
	void HandleLoginRes(const se::auth::S_LoginRes& Pkt);
	void HandlePong(const se::auth::S_Pong& Pkt);
	void HandleSetNicknameRes(const se::lobby::S_SetNicknameRes& Pkt);
	void HandleMatchQueueEnterRes(const se::lobby::S_MatchQueueEnterRes& Pkt);
	void HandleMatchQueueCancelRes(const se::lobby::S_MatchQueueCancelRes& Pkt);
	void HandleMatchFound(const se::lobby::N_MatchFound& Pkt);
	void HandleRoomEnterRes(const se::room::S_RoomEnterRes& Pkt);
	void HandleRoomLeaveRes(const se::room::S_RoomLeaveRes& Pkt);
	void HandleEntitySpawn(const se::room::N_EntitySpawn& Pkt);
	void HandleEntityDespawn(const se::room::N_EntityDespawn& Pkt);
	void HandleRoomClosed(const se::room::N_RoomClosed& Pkt);
	void HandleGameStart(const se::game::N_GameStart& Pkt);
	void HandleGameEnd(const se::game::N_GameEnd& Pkt);
	void HandleMove(const se::game::N_Move& Pkt);
	void HandleJump(const se::game::N_Jump& pkt);
	void HandleJumpLand(const se::game::N_JumpLand& pkt);
	void HandleCrouch(const se::game::N_Crouch& pkt);
	void HandleWireAction(const se::game::N_WireAction& pkt);
	void HandleWireActionEnd(const se::game::N_WireActionEnd& pkt);
	void HandleAim(const se::game::N_Aim& pkt);
	void HandleFire(const se::game::N_Fire& Pkt);
	void HandleAttack(const se::game::N_Attack& Pkt);
	void HandleThrowGrenade(const se::game::N_ThrowGrenade& Pkt);
	void HandleReload(const se::game::N_Reload& Pkt);
	void HandleWeaponChanged(const se::game::N_WeaponChanged& Pkt);
	void HandleUseAbility(const se::game::N_UseAbility& Pkt);
	void HandleKillPlayer(const se::game::N_KillPlayer& Pkt);
	void HandleUseItem(const se::game::N_UseItem& Pkt);
	void HandlePickupItem(const se::game::N_PickupItem& Pkt);
	void HandleUseStoreRes(const se::game::S_UseStoreRes& Pkt);
	void HandleItemGained(const se::game::N_ItemGained& Pkt);
	void HandleHealthChanged(const se::game::N_HealthChanged& Pkt);
	void HandleEntityDied(const se::game::N_EntityDied& Pkt);
	void HandleEntityRespawned(const se::game::N_EntityRespawned& Pkt);
	void HandleEntityDestroyed(const se::game::N_EntityDestroyed& Pkt);
	void HandleTimePointChanged(const se::game::N_TimePointChanged& Pkt);
	void HandleTimeStormChange(const se::game::N_TimeStormChange& Pkt);
	
private:
	void RemoveEntity(uint32 EntityId);
	
private:
	AActor* FindEntityActor(uint32 EntityId) const;
	AActor* SpawnEntityActor(const FNetworkEntityState& EntityState);
	AActor* GetOrSpawnEntityActor(uint32 EntityId);

	void InitializeSpawnedPawnData(AActor* Actor);
	void PostSpawnEntityActor(AActor* SpawnedActor, const FNetworkEntityState& EntityState);
	void InitializeNetworkEntityActor(AActor* SpawnedActor, const FNetworkEntityState& EntityState);
	void ApplyRuntimeConfigToActor(AActor* Actor);
	void HandleLocalPlayerActorSpawned(AActor* SpawnedActor, const FNetworkEntityState& EntityState);
	
private:
	void ApplyEntityStateToActor(uint32 EntityId);
	void ApplyEntityStateToActor(AActor* Actor, const FNetworkEntityState& EntityState);
	void ApplyAllEntityStates();
	
	void ApplyRemoteAttackNotifyToActor(uint32 EntityId, const FRemoteAttackNotify& Notify);
	
	bool IsLocalPlayerEntity(uint32 EntityId) const;
	TSubclassOf<AActor> ResolveActorClass(const FNetworkEntityState& EntityState) const;
	
private:
	bool LoadClientConfig();
	
public:
	UFUNCTION(BlueprintCallable, Category = "Network|Lobby")
	void RequestSetNickname(const FString& Nickname);
	
	UFUNCTION(BlueprintCallable, Category = "Network|Lobby")
	void RequestMatchQueueEnter();
	
	UFUNCTION(BlueprintCallable, Category = "Network|Lobby")
	void RequestMatchQueueCancel();
	
	UFUNCTION(BlueprintCallable, Category = "Network|Room")
	void RequestEnterRoom();
	
	UFUNCTION(BlueprintCallable, Category = "Network|Room")
	void RequestLeaveRoom();
	
	UFUNCTION(BlueprintCallable, Category = "Network|Room")
	void RequestLoadingComplete();
	
public:
	void Ping();
	
private:
	void StartPingTimer();
	void StopPingTimer();
	
private:
	void ClearRoomState();
	
public:
	const UTimeThiefPawnData* GetDefaultPawnData() const { return DefaultLocalPlayerPawnData; }
	
public:
	UPROPERTY(EditDefaultsOnly, Category = "Network|Spawn")
	TObjectPtr<USpawnClassData> SpawnData;
	
	UPROPERTY(EditDefaultsOnly, Category = "Network|Spawn")
	TObjectPtr<const UTimeThiefPawnData> DefaultLocalPlayerPawnData;
	
private:
	bool bDisconnecting = false;
	bool bIsConnected = false;
	FSocket* Socket = nullptr;
	
	TSharedPtr<PacketSession> GameSession;
	
	FTimerHandle QueueProcessingTimer;
	FTimerHandle PingTimer;
	
	FClientConfig ClientConfig;
	
private:
	UPROPERTY(BlueprintReadOnly, Category = "Network", meta=(AllowPrivateAccess="true"))
	ENetworkPlayState PlayState = ENetworkPlayState::Disconnected;
	
	FLocalPlayerInfo LocalPlayerInfo;
	
	uint32 LocalPlayerEntityId = 0;
	uint32 TryRoomId = 0;
	
	UPROPERTY()
	FRuntimeConfig RuntimeConfig;
	
private:
	FRoomState RoomState;
	
	TMap<uint32, FEntityRuntimeEntry> EntityEntries;
	
};
