// Fill out your copyright notice in the Description page of Project Settings.
#include "Network/NetworkGameInstanceSubsystem.h"

#include <Generated/ClientPacketHandler.h>

#include "SocketSubsystem.h"
#include "Interfaces/IPv4/IPv4Address.h"

#include "PacketSession.h"
#include "ClientConfigLoader.h"

/*---------------------------------
   NetworkGameInstanceSubsystem
---------------------------------*/

void UNetworkGameInstanceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	LocalPlayerClass = LoadClass<AActor>(nullptr, TEXT("/Game/SSH/BP_NTLocalPlayer.BP_NTLocalPlayer_C"));
	RemotePlayerClass = LoadClass<AActor>(nullptr, TEXT("/Game/SSH/BP_NTPlayer.BP_NTPlayer_C"));
	
	bool configLoaded = LoadClientConfig();
	
	if (!configLoaded)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load client config. Check the file path and format."));
		return;
	}
	
	ConnectToServer(ClientConfig.ServerIp, ClientConfig.ServerPort);
	
	SpawnProcessPacketTimer();
}

void UNetworkGameInstanceSubsystem::Deinitialize()
{
	DisconnectFromServer();
	
	Super::Deinitialize();
}

UNetworkGameInstanceSubsystem* UNetworkGameInstanceSubsystem::Get(UObject* WorldContextObject)
{
	// TEMP
	
	if (!WorldContextObject) return nullptr;
	
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			return GameInstance->GetSubsystem<UNetworkGameInstanceSubsystem>();
		}
	}
	return nullptr;
}

void UNetworkGameInstanceSubsystem::SendPacket(TSharedPtr<SendBuffer> Buffer)
{
	if (not bIsConnected or GameSession == nullptr) return;
	
	GameSession->SendPacket(Buffer);
}

void UNetworkGameInstanceSubsystem::ConnectToServer(const FString& IPAddress, int32 Port)
{
	Socket = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateSocket(NAME_Stream, TEXT("Client Socket"));
	if (Socket == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[Networkd] Failed to create socket"));
		return;
	}
	
	FIPv4Address ip;
	if (FIPv4Address::Parse(IPAddress, ip) == false)
	{
		UE_LOG(LogTemp, Error, TEXT("[Network] Invalid IP address: %s"), *IPAddress);
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
		Socket = nullptr;
		return;
	}
	
	TSharedPtr<FInternetAddr> addr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
	addr->SetIp(ip.Value);
	addr->SetPort(Port);

	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("Connecting to Server...")));
	
	bool connected = Socket->Connect(*addr);
	
	if (connected)
	{
		bIsConnected = true;
		
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, FString::Printf(TEXT("Connection Success")));
		
		GameSession = MakeShared<PacketSession>(Socket);
		GameSession->Run();
		
		// se::lobby::C_LobbyEnterReq lobbyEnterReq;
		// auto packet = ClientPacketHandler::MakeSendBuffer(lobbyEnterReq);
		// SendPacket(packet);
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("Connection Failed")));
		
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
		Socket = nullptr;
		bIsConnected = false;
	}
}

void UNetworkGameInstanceSubsystem::DisconnectFromServer()
{
	// // 이미 연결이 끊겼거나 소켓이 유효하지 않은 경우에는 아무 작업도 수행하지 않습니다.
	// if (bIsConnected == false or Socket == nullptr) return;
	
	UE_LOG(LogTemp, Log, TEXT("Disconnecting from server..."));
	
	// 타이머 정지 (패킷 처리 타이머)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(QueueProcessingTimer);
	}
	
	// 세션 종료 (Worker 스레드 종료 포함)
	if (GameSession.IsValid())
	{
		GameSession->Disconnect();
		GameSession.Reset();
	}
	
	// 소켓 닫기 및 정리
	if (Socket)
	{
		Socket->Shutdown(ESocketShutdownMode::ReadWrite);
		Socket->Close();
		
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
		Socket = nullptr;
	}
	
	// 모든 Entity Actor 제거
	for  (auto& Pair : EntityActors)
	{
		if (Pair.Value.IsValid())
		{
			Pair.Value->Destroy();
		}
	}
	
	EntityActors.Empty();
	NetworkEntities.Empty();
	
	bIsConnected = false;
	LocalPlayerEntityId = 0;
	
	LocalPlayerInfo.Reset();
	RoomState.Reset();
	
	UE_LOG(LogTemp, Log, TEXT("Disconnected and cleaned up"));
}

void UNetworkGameInstanceSubsystem::SpawnProcessPacketTimer()
{
	if (not bIsConnected) return;
	
	if (UWorld* World = GetWorld())
	{
		// TODO: 0.1초 값은 .ini나 .config 파일로 부터 읽어와서 적용해야 할 듯 싶다
		World->GetTimerManager().SetTimer(QueueProcessingTimer, this, &UNetworkGameInstanceSubsystem::ProcessPacket, 0.1f, true);
	}
}

void UNetworkGameInstanceSubsystem::ProcessPacket()
{
	if (not bIsConnected or GameSession == nullptr) return;
	
	GameSession->HandleRecvPackets();
}

void UNetworkGameInstanceSubsystem::HandleHandshakeRes(const se::auth::S_HandshakeRes& pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleLoginRes(const se::auth::S_LoginRes& pkt)
{
}

void UNetworkGameInstanceSubsystem::HandlePong(const se::auth::S_Pong& pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleSetNicknameRes(const se::lobby::S_SetNicknameRes& pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleMatchQueueEnterRes(const se::lobby::S_MatchQueueEnterRes& pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleMatchQueueCancelRes(const se::lobby::S_MatchQueueCancelRes& pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleMatchFound(const se::lobby::N_MatchFound& pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleRoomEnterRes(const se::room::S_RoomEnterRes& pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleRoomLeaveRes(const se::room::S_RoomLeaveRes& pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleEntitySpawn(const se::room::N_EntitySpawn& pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleEntityDespawn(const se::room::N_EntityDespawn& pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleRoomClosed(const se::room::N_RoomClosed& pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleGameStart(const se::game::N_GameStart& pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleGameEnd(const se::game::N_GameEnd& pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleMove(const se::game::N_Move& pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleFire(const se::game::N_Fire& pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleAttack(const se::game::N_Attack& pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleThrowGrenade(const se::game::N_ThrowGrenade& pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleReload(const se::game::N_Reload& pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleWeaponChanged(const se::game::N_WeaponChanged& pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleUseAbility(const se::game::N_UseAbility& pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleKillPlayer(const se::game::N_KillPlayer& pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleUseItem(const se::game::N_UseItem& pkt)
{
}

void UNetworkGameInstanceSubsystem::HandlePickupItem(const se::game::N_PickupItem& pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleUseStoreRes(const se::game::S_UseStoreRes& pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleItemGained(const se::game::N_ItemGained& pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleHealthChanged(const se::game::N_HealthChanged& pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleEntityDied(const se::game::N_EntityDied& pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleEntityRespawned(const se::game::N_EntityRespawned& pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleEntityDestroyed(const se::game::N_EntityDestroyed& pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleTimePointChanged(const se::game::N_TimePointChanged& pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleTimeStormChange(const se::game::N_TimeStormChange& pkt)
{
}

bool UNetworkGameInstanceSubsystem::IsLocalPlayerEntity(uint32 EntityId) const
{
	return LocalPlayerEntityId != 0 && LocalPlayerEntityId == EntityId;
}

TSubclassOf<AActor> UNetworkGameInstanceSubsystem::ResolveActorClass(const FNetworkEntityState& EntityState) const
{
	// TODO: EntityState의 정보를 바탕으로 어떤 Actor 클래스를 스폰할지 결정하는 로직을 구현해야 한다
	//		 어떤 ObjectType, TemplateId 여도 처리할 수 있도록 (조합가능한 기준)
	
	if (IsLocalPlayerEntity(EntityState.EntityId))
	{
		return LocalPlayerClass;
	}
	
	return RemotePlayerClass;
}

bool UNetworkGameInstanceSubsystem::LoadClientConfig()
{
	const FString FilePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("../External/ProtocolShared/Config/client.dev.json"));
	
	UE_LOG(LogTemp, Log, TEXT("[Config] Loading client config from %s"), *FilePath);
	
	return FClientConfigLoader::LoadClientConfigFromFile(FilePath, ClientConfig);
}

void UNetworkGameInstanceSubsystem::RequestEnterRoom()
{
	if (bIsConnected == false || GameSession == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] Cannot request to enter room: Not connected to server"));
		return;
	}
	
	se::room::C_RoomEnterReq Request;
	Request.set_room_id(1); // TEMP;
	
	auto SendBuffer = ClientPacketHandler::MakeSendBuffer(Request);
	SendPacket(SendBuffer);
	
	UE_LOG(LogTemp, Log, TEXT("[Network] Sent C_RoomEnterReq to server"));
}

void UNetworkGameInstanceSubsystem::RequestLeaveRoom()
{
	if (bIsConnected == false || GameSession == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] Cannot request to leave room: Not connected to server"));
		return;
	}
	
	se::room::C_RoomLeaveReq Request;
	
	auto SendBuffer = ClientPacketHandler::MakeSendBuffer(Request);
	SendPacket(SendBuffer);
	
	UE_LOG(LogTemp, Log, TEXT("[Network] Sent C_RoomLeaveReq to server"));
}

void UNetworkGameInstanceSubsystem::RequestLoadingComplete()
{
	if (bIsConnected == false || GameSession == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] Cannot request loading complete: Not connected to server"));
		return;
	}
	
	se::game::C_LoadingCompleteReq Request;
	auto SendBuffer = ClientPacketHandler::MakeSendBuffer(Request);
	SendPacket(SendBuffer);
	
	UE_LOG(LogTemp, Log, TEXT("[Network] Sent C_LoadingCompleteReq to server"));
}

void UNetworkGameInstanceSubsystem::AddEntity(uint32 EntityId, AActor* Actor)
{
	if (EntityId == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] AddEntity failed: Invalid EntityId (0)"));
		return;
	}
	
	if (Actor == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] AddEntity failed: Actor is null for EntityId %u"), EntityId);
		return;
	}
	
	if (Actor->IsPendingKillPending())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] AddEntity failed: Actor is pending kill (EntityId=%u)"), EntityId);
		return;
	}
	
	if (EntityActors.Contains(EntityId))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] AddEntity: EntityId %u already exists, replacing Actor"), EntityId);
	}
	
	EntityActors.Add(EntityId, Actor);
	
	UE_LOG(LogTemp, Log, TEXT("[Network] AddEntity: EntityId=%u, Actor=%s"), EntityId, *Actor->GetName());
}

void UNetworkGameInstanceSubsystem::RemoveEntityState(uint32 EntityId)
{
	if (EntityId == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] RemoveEntityState failed: Invalid EntityId"));
		return;
	}
	
	const int32 RemovedCount = NetworkEntities.Remove(EntityId);
	
	UE_LOG(LogTemp, Log, TEXT("[Network] RemoveEntityState: EntityId=%u, Removed=%d"), EntityId, RemovedCount);
}

AActor* UNetworkGameInstanceSubsystem::FindEntityActor(uint32 EntityId) const
{
	const TWeakObjectPtr<AActor>* FoundActor = EntityActors.Find(EntityId);
	if (FoundActor == nullptr) return nullptr;
	
	return FoundActor->Get();
}

AActor* UNetworkGameInstanceSubsystem::SpawnEntityActor(const FNetworkEntityState& EntityState)
{
	UWorld* World = GetWorld();
	if (World == nullptr) return nullptr;
	
	// TODO: World가 GameWorld가 아니면 안된다 (문제가 생긴 것)
	
	TSubclassOf<AActor> ActorClass = ResolveActorClass(EntityState);
	if (*ActorClass == nullptr) return nullptr;
	
	const FRotator SpawnRotation(0.0f, EntityState.Yaw, 0.0f);
	const FTransform SpawnTransform(SpawnRotation, EntityState.Position);
	
	AActor* SpawnedActor = World->SpawnActor<AActor>(ActorClass, SpawnTransform);
	if (SpawnedActor == nullptr) return nullptr;
	
	AddEntity(EntityState.EntityId, SpawnedActor);
	return SpawnedActor;
}

void UNetworkGameInstanceSubsystem::DestroyEntityActor(uint32 EntityId)
{
	if (EntityId == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] DestroyEntityActor failed: Invalid EntityId"));
		return;
	}
	
	TWeakObjectPtr<AActor>* FoundActor = EntityActors.Find(EntityId);
	if (FoundActor == nullptr)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[Network] DestroyEntityActor: Actor not found (EntityId=%u)"), EntityId);
		return;
	}
	
	if (FoundActor->IsValid())
	{
		AActor* Actor = FoundActor->Get();
		if (Actor != nullptr)
		{
			UE_LOG(LogTemp, Log, TEXT("[Network] DestroyEntityActor: EntityId=%u, Actor=%s"), EntityId, *Actor->GetName());

			Actor->Destroy();
		}
	}

	EntityActors.Remove(EntityId);
}

AActor* UNetworkGameInstanceSubsystem::GetOrSpawnEntityActor(uint32 EntityId)
{
	if (AActor* ExistingActor = FindEntityActor(EntityId))
	{
		return ExistingActor;
	}
	
	const FNetworkEntityState* EntityState = NetworkEntities.Find(EntityId);
	if (EntityState == nullptr) return nullptr;
	
	return SpawnEntityActor(*EntityState);
}

void UNetworkGameInstanceSubsystem::ApplyEntityStateToActor(uint32 EntityId)
{
	const FNetworkEntityState* EntityState = NetworkEntities.Find(EntityId);
	if (EntityState == nullptr) return;
	
	AActor* Actor = GetOrSpawnEntityActor(EntityId);
	if (Actor == nullptr) return;
	
	ApplyEntityStateToActor(Actor, *EntityState);
}

void UNetworkGameInstanceSubsystem::ApplyEntityStateToActor(AActor* Actor, const FNetworkEntityState& EntityState)
{
	if (Actor == nullptr) return;
	
	FRotator NewRotation(0.0f, EntityState.Yaw, 0.0f);
	
	// TODO: 좀더 다른 방향으로 State를 전달해야 한다
	// 임시로 NTPlayer에 함수를 추가하고 해당 객체로 캐스팅하여 함수 호출 하는 방식
	// 다음에는 interface 등 더 우아한 방법으로 교체하도록
	// 구 코드	
	// --------------------------------------------
	// if (ANTPlayer* Player = Cast<ANTPlayer>(Actor))
	// {
	// 	Player->SetDestPosition(EntityState.Position);
	// 	Player->SetTargetYaw(EntityState.Yaw);
	// 	Player->SetTargetPitch(EntityState.Pitch);
	// }
	// else
	// {
	// 	Actor->SetActorLocation(EntityState.Position);
	// 	Actor->SetActorRotation(NewRotation);
	// }
	
	// TEMP
	Actor->SetActorLocation(EntityState.Position);
	Actor->SetActorRotation(NewRotation);
}

void UNetworkGameInstanceSubsystem::ApplyAllEntityStates()
{
	for (const TPair<uint32, FNetworkEntityState>& Pair : NetworkEntities)
	{
		const uint32 EntityId = Pair.Key;
		ApplyEntityStateToActor(EntityId);
	}
}
