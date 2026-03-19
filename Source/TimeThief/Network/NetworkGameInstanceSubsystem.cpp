// Fill out your copyright notice in the Description page of Project Settings.
#include "Network/NetworkGameInstanceSubsystem.h"

#include <Generated/ClientPacketHandler.h>

#include "SocketSubsystem.h"
#include "Interfaces/IPv4/IPv4Address.h"

#include "Protocol.pb.h"

#include "PacketSession.h"
#include "ClientConfigLoader.h"
#include "NetworkEntityComponent.h"
#include "Network/State/MoveSyncData.h"
#include "Network/TestPlayer/NTLocalPlayer.h"

namespace
{
	static bool IsRoomPlayableState(ENetworkPlayState State)
	{
		return State == ENetworkPlayState::InRoom;
	}
}

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

void UNetworkGameInstanceSubsystem::SendMove(const FMoveSyncData& MoveData)
{
	se::game::C_MoveReq Pkt;
	
	auto* Movement = Pkt.mutable_movement();
	auto* Position = Movement->mutable_position();
	Position->set_x(MoveData.Position.X);
	Position->set_y(MoveData.Position.Y);
	Position->set_z(MoveData.Position.Z);
	
	Movement->set_yaw(MoveData.Yaw);
	Movement->set_pitch(MoveData.Pitch);
	
	auto Buffer = ClientPacketHandler::MakeSendBuffer(Pkt);
	SendPacket(Buffer);
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
		PlayState = ENetworkPlayState::Connected;
		
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
	
	ClearRoomState();
	LocalPlayerInfo = FLocalPlayerInfo();
	
	PlayState = ENetworkPlayState::Disconnected;
	
	bIsConnected = false;
	
	UE_LOG(LogTemp, Log, TEXT("Disconnected and cleaned up"));
}

void UNetworkGameInstanceSubsystem::SpawnProcessPacketTimer()
{
	if (not bIsConnected) return;
	
	if (UWorld* World = GetWorld())
	{
		// TODO: 0.05초 값은 .ini나 .config 파일로 부터 읽어와서 적용해야 할 듯 싶다
		World->GetTimerManager().SetTimer(QueueProcessingTimer, this, &UNetworkGameInstanceSubsystem::ProcessPacket, 0.05f, true);
	}
}

void UNetworkGameInstanceSubsystem::ProcessPacket()
{
	check(IsInGameThread());
	
	if (not bIsConnected or GameSession == nullptr) return;
	
	GameSession->HandleRecvPackets();
}

void UNetworkGameInstanceSubsystem::HandleHandshakeRes(const se::auth::S_HandshakeRes& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleLoginRes(const se::auth::S_LoginRes& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandlePong(const se::auth::S_Pong& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleSetNicknameRes(const se::lobby::S_SetNicknameRes& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleMatchQueueEnterRes(const se::lobby::S_MatchQueueEnterRes& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleMatchQueueCancelRes(const se::lobby::S_MatchQueueCancelRes& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleMatchFound(const se::lobby::N_MatchFound& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleRoomEnterRes(const se::room::S_RoomEnterRes& Pkt)
{
	check(IsInGameThread());
	
	if (!Pkt.success())
	{
		const auto& Result = Pkt.result();
		
		UE_LOG(LogTemp, Warning, TEXT("Failed to enter room: %s"), UTF8_TO_TCHAR(Result.message().c_str()));
		PlayState = ENetworkPlayState::Connected;
		return;
	}
	
	if (!Pkt.has_my_entity_id())
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to enter room: Missing my_entity_id in response"));
		PlayState = ENetworkPlayState::Connected;
		return;
	}
	
	if (!Pkt.has_snapshot())
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to enter room: Missing room snapshot in response"));
		PlayState = ENetworkPlayState::Connected;
		return;
	}
	
	ClearRoomState();
	
	const auto& Snapshot = Pkt.snapshot();
	
	LocalPlayerEntityId = Pkt.my_entity_id().value();
	RoomState.RoomId = Snapshot.room_id();
	
	for (const auto& PlayerInfo : Snapshot.players())
	{
		if (!PlayerInfo.has_player_id() || !PlayerInfo.has_entity_id())
		{
			UE_LOG(LogTemp, Warning, TEXT("[Network] Skip invalid player info in room snapshot"));
			continue;
		}
		
		FRoomPlayerInfo Info;
		Info.PlayerId = PlayerInfo.player_id().value();
		Info.EntityId = PlayerInfo.entity_id().value();
		Info.Nickname = UTF8_TO_TCHAR(PlayerInfo.nickname().c_str());
		
		RoomState.Players.Add(Info);
	}

	PlayState = ENetworkPlayState::InRoom;
	
	UE_LOG(LogTemp, Log, TEXT("[Network] Room enter success. RoomId=%u, LocalEntityId=%u"), RoomState.RoomId, LocalPlayerEntityId);
	RequestLoadingComplete();	// TEMP
}

void UNetworkGameInstanceSubsystem::HandleRoomLeaveRes(const se::room::S_RoomLeaveRes& Pkt)
{
	check(IsInGameThread());
	
	if (!Pkt.success())
	{
		const auto& Result = Pkt.result();
		
		UE_LOG(LogTemp, Warning, TEXT("Failed to leave room: %s"), UTF8_TO_TCHAR(Result.message().c_str()));
		PlayState = ENetworkPlayState::InRoom;
		return;
	}
	
	ClearRoomState();
	PlayState = ENetworkPlayState::Connected;
	
	UE_LOG(LogTemp, Log, TEXT("[Network] Room leave success"));
}

void UNetworkGameInstanceSubsystem::HandleEntitySpawn(const se::room::N_EntitySpawn& Pkt)
{
	check(IsInGameThread());
	
	if (!IsRoomPlayableState(PlayState))
	{
		return;
	}
	
	if (!Pkt.has_info())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] HandleEntitySpawn: Missing info"));
		return;
	}
	
	const auto& Info = Pkt.info();
	const uint32 EntityId = Info.entity_id().value();
	
	FNetworkEntityState& EntityState = NetworkEntities.FindOrAdd(EntityId);
	EntityState.EntityId = EntityId;
	
	EntityState.ObjectType = Info.type();
	EntityState.TemplateId = Info.template_id();
	const auto& Movement = Info.movement();
	const auto& Pos = Movement.position();
	EntityState.Position = FVector(Pos.x(), Pos.y(), Pos.z());
	EntityState.Yaw = Movement.yaw();
	EntityState.Pitch = Movement.pitch();
	
	ApplyEntityStateToActor(EntityId);
	
	UE_LOG(LogTemp, Log, TEXT("[Network] Entity spawned: EntityId=%u"), EntityId);
}

void UNetworkGameInstanceSubsystem::HandleEntityDespawn(const se::room::N_EntityDespawn& Pkt)
{
	check(IsInGameThread());
	
	if (!IsRoomPlayableState(PlayState))
	{
		return;
	}
	
	const uint32 EntityId = Pkt.entity_id().value();
	
	DestroyEntityActor(EntityId);
	RemoveEntityState(EntityId);
	
	if (EntityId == LocalPlayerEntityId)
	{
		LocalPlayerEntityId = 0;
	}
	
	UE_LOG(LogTemp, Log, TEXT("[Network] Entity despawned: EntityId=%u"), EntityId);
}

void UNetworkGameInstanceSubsystem::HandleRoomClosed(const se::room::N_RoomClosed& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleGameStart(const se::game::N_GameStart& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleGameEnd(const se::game::N_GameEnd& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleMove(const se::game::N_Move& Pkt)
{
	check(IsInGameThread());
	
	if (!IsRoomPlayableState(PlayState))
	{
		return;
	}
	
	const uint32 EntityId = Pkt.entity_id().value();
	FNetworkEntityState* EntityState = NetworkEntities.Find(EntityId);
	if (EntityState == nullptr)
	{
		return;
	}
	
	const auto& Movement = Pkt.movement();
	const auto& Pos = Movement.position();
	EntityState->Position = FVector(Pos.x(), Pos.y(), Pos.z());
	EntityState->Yaw = Movement.yaw();
	EntityState->Pitch = Movement.pitch();
	
	ApplyEntityStateToActor(EntityId);
}

void UNetworkGameInstanceSubsystem::HandleFire(const se::game::N_Fire& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleAttack(const se::game::N_Attack& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleThrowGrenade(const se::game::N_ThrowGrenade& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleReload(const se::game::N_Reload& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleWeaponChanged(const se::game::N_WeaponChanged& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleUseAbility(const se::game::N_UseAbility& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleKillPlayer(const se::game::N_KillPlayer& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleUseItem(const se::game::N_UseItem& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandlePickupItem(const se::game::N_PickupItem& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleUseStoreRes(const se::game::S_UseStoreRes& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleItemGained(const se::game::N_ItemGained& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleHealthChanged(const se::game::N_HealthChanged& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleEntityDied(const se::game::N_EntityDied& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleEntityRespawned(const se::game::N_EntityRespawned& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleEntityDestroyed(const se::game::N_EntityDestroyed& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleTimePointChanged(const se::game::N_TimePointChanged& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleTimeStormChange(const se::game::N_TimeStormChange& Pkt)
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
	
	if (PlayState != ENetworkPlayState::Connected)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] Cannot request to enter room: Invalid state"));
		return;
	}
	
	se::room::C_RoomEnterReq Request;
	Request.set_room_id(1); // TEMP;
	
	auto SendBuffer = ClientPacketHandler::MakeSendBuffer(Request);
	SendPacket(SendBuffer);
	
	PlayState = ENetworkPlayState::EnteringRoom;
	
	UE_LOG(LogTemp, Log, TEXT("[Network] Sent C_RoomEnterReq to server"));
}

void UNetworkGameInstanceSubsystem::RequestLeaveRoom()
{
	if (bIsConnected == false || GameSession == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] Cannot request to leave room: Not connected to server"));
		return;
	}
	
	if (PlayState != ENetworkPlayState::InRoom)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] Cannot request to leave room: Invalid state"));
		return;
	}
	
	se::room::C_RoomLeaveReq Request;
	
	auto SendBuffer = ClientPacketHandler::MakeSendBuffer(Request);
	SendPacket(SendBuffer);
	
	PlayState = ENetworkPlayState::LeavingRoom;
	
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

void UNetworkGameInstanceSubsystem::ClearRoomState()
{
	for (auto& Pair : EntityActors)
	{
		if (Pair.Value.IsValid())
		{
			Pair.Value->Destroy();
		}
	}

	EntityActors.Empty();
	NetworkEntities.Empty();
	LocalPlayerEntityId = 0;
	RoomState = FRoomState();
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
	
	if (!IsValid(Actor))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] AddEntity failed: Actor is invalid for EntityId %u"), EntityId);
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
	PostSpawnEntityActor(SpawnedActor, EntityState);
	
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

void UNetworkGameInstanceSubsystem::PostSpawnEntityActor(AActor* SpawnedActor, const FNetworkEntityState& EntityState)
{
	if (SpawnedActor == nullptr) return;
	
	InitializeNetworkEntityActor(SpawnedActor, EntityState);;
	
	if (IsLocalPlayerEntity(EntityState.EntityId))
	{
		HandleLocalPlayerActorSpawned(SpawnedActor, EntityState);
	}
}

void UNetworkGameInstanceSubsystem::InitializeNetworkEntityActor(AActor* SpawnedActor, const FNetworkEntityState& EntityState)
{
	if (SpawnedActor == nullptr) return;
	
	UNetworkEntityComponent* NetworkEntityComp = SpawnedActor->FindComponentByClass<UNetworkEntityComponent>();
	if (NetworkEntityComp == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] Spawned actor has no NetworkEntityComponent: %s"), *GetNameSafe(SpawnedActor));
		return;
	}
	
	NetworkEntityComp->SetEntityId(EntityState.EntityId);
	switch (EntityState.ObjectType)
	{
	case se::common::OBJ_PLAYER:
		if (IsLocalPlayerEntity(EntityState.EntityId))
		{
			NetworkEntityComp->SetControlType(ENetworkControlType::Local);
		}
		else
		{
			NetworkEntityComp->SetControlType(ENetworkControlType::Remote);
		}
		break;
	default:
		break;
	}
}

void UNetworkGameInstanceSubsystem::HandleLocalPlayerActorSpawned(AActor* SpawnedActor,
                                                                  const FNetworkEntityState& EntityState)
{
	UWorld* World = GetWorld();
	if (World == nullptr || SpawnedActor == nullptr) return;
	
	APawn* SpawnPawn = Cast<APawn>(SpawnedActor);
	if (SpawnPawn == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] Local player actor is not a Pawn. EntityId=%u"), EntityState.EntityId);
		return;
	}
	
	APlayerController* PC = World->GetFirstPlayerController();
	if (PC == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] Failed to possess local player: PlayerController is null"));
		return;
	}
	
	TWeakObjectPtr<APlayerController> WeakPC = PC;
	TWeakObjectPtr<APawn> WeakPawn = SpawnPawn;
	
	World->GetTimerManager().SetTimerForNextTick([WeakPC, WeakPawn]()
	{
		if (!WeakPC.IsValid() || !WeakPawn.IsValid()) return;
		
		APlayerController* LocalPC = WeakPC.Get();
		APawn* NewPawn = WeakPawn.Get();
		
		if (APawn* OldPawn = LocalPC->GetPawn())
		{
			if (OldPawn != NewPawn)
			{
				LocalPC->UnPossess();
			}
		}
		
		LocalPC->Possess(NewPawn);
		LocalPC->SetViewTarget(NewPawn);
	});
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
	
	if (IMovableNetworkEntityInterface* Movable = Cast<IMovableNetworkEntityInterface>(Actor))
	{
		Movable->ApplyNetworkMovementState(EntityState);
		return;
	}
	
	const FRotator NewRotation{0.0f, EntityState.Yaw, 0.0f};
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
