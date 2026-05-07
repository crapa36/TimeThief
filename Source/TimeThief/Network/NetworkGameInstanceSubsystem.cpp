// Fill out your copyright notice in the Description page of Project Settings.
#include "Network/NetworkGameInstanceSubsystem.h"

#include <Components/Skill/SavePointSkillComponent.h>
#include <Generated/ClientPacketHandler.h>

#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Interfaces/IPv4/IPv4Address.h"

#include "Protocol.pb.h"

#include "PacketSession.h"
#include "ClientConfigLoader.h"
#include "NetworkEntityComponent.h"
#include "NetworkMoveComponent.h"
#include "TimeThiefGameplayTags.h"
#include "TimeThiefNetworkSettings.h"
#include "Actors/Item/ItemBase.h"
#include "Character/TimeThiefPlayerCharacter.h"
#include "Character/TimeThiefPlayerController.h"
#include "Components/TimeThiefHealthComponent.h"
#include "Components/Combat/TimeThiefPlayerCombatComponent.h"
#include "Components/System/TimePointSystemComponent.h"
#include "Components/System/TimeStormComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Microsoft/AllowMicrosoftPlatformTypes.h"
#include "Network/State/MoveSyncData.h"
#include "Network/State/EntityRuntimeEntry.h"
#include "Network/TestPlayer/NTLocalPlayer.h"
#include "Protocol/ProtocolVersion.h"
#include "State/RemoteAttackNotify.h"
#include "Network/NetworkCombatSyncComponent.h"
#include "State/NetworkActionTypes.h"
#include "Components/Wire/TimeThiefWireComponent.h"
#include "Game/ItemPoolWorldSubsystem.h"
#include "Game/ItemSettings.h"
#include "Utils/TimeThiefAimStatics.h"
#include "Weapon/TimeThiefMasterWeapon.h"
#include "Weapon/TimeThiefRocketProjectile.h"
#include "Weapon/Components/TimeThiefRocketLauncherComponent.h"
#include "Weapon/Components/TimeThiefWeaponComponentBase.h"

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
	
	const UTimeThiefNetworkSettings* Settings = GetDefault<UTimeThiefNetworkSettings>();
	if (Settings == nullptr)
	{
		return;
	}
	
	if (!Settings->SpawnClassData.IsNull())
	{
		SpawnData = Settings->SpawnClassData.LoadSynchronous();
	}
	
	if (!Settings->DefaultLocalPlayerPawnData.IsNull())
	{
		DefaultLocalPlayerPawnData = Settings->DefaultLocalPlayerPawnData.LoadSynchronous();
	}
	
	bool configLoaded = LoadClientConfig();
	
	if (!configLoaded)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load client config. Check the file path and format."));
		return;
	}
	
	ConnectToServer();
	
	if (bIsConnected)
	{
		Handshaking();
		
		SpawnProcessPacketTimer();
	}
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
	
	Movement->set_yaw(MoveData.CharYaw);
	Movement->set_aim_yaw(MoveData.AimYaw);
	Movement->set_pitch(MoveData.AimPitch);
	auto* Velocity = Movement->mutable_velocity();
	Velocity->set_x(MoveData.Velocity.X);
	Velocity->set_y(MoveData.Velocity.Y);
	Movement->set_movement_mode(MoveData.MovementMode);
	
	auto Buffer = ClientPacketHandler::MakeSendBuffer(Pkt);
	SendPacket(Buffer);
}

void UNetworkGameInstanceSubsystem::SendJump()
{
	se::game::C_JumpReq Request;
	auto Buffer = ClientPacketHandler::MakeSendBuffer(Request);
	SendPacket(Buffer);
}

void UNetworkGameInstanceSubsystem::SendDoubleJump()
{
	se::game::C_DoubleJumpReq Request;
	auto Buffer = ClientPacketHandler::MakeSendBuffer(Request);
	SendPacket(Buffer);
}


void UNetworkGameInstanceSubsystem::SendJumpLand()
{
	se::game::C_JumpLand Request;
	auto Buffer = ClientPacketHandler::MakeSendBuffer(Request);
	SendPacket(Buffer);
}

void UNetworkGameInstanceSubsystem::SendWireAction(const FVector& AnchorPoint)
{
	se::game::C_WireActionReq Request;
	auto* Anchor = Request.mutable_anchor_point();
	Anchor->set_x(AnchorPoint.X);
	Anchor->set_y(AnchorPoint.Y);
	Anchor->set_z(AnchorPoint.Z);

	UE_LOG(LogTemp, Log, TEXT("[WirePkt][Stage=Send][C_WireActionReq] Anchor=(%.1f, %.1f, %.1f)"), AnchorPoint.X, AnchorPoint.Y, AnchorPoint.Z);

	auto Buffer = ClientPacketHandler::MakeSendBuffer(Request);
	SendPacket(Buffer);
}

void UNetworkGameInstanceSubsystem::SendWireActionEnd()
{
	se::game::C_WireActionEnd Request;
	UE_LOG(LogTemp, Log, TEXT("[WirePkt][Stage=Send][C_WireActionEnd]"));
	auto Buffer = ClientPacketHandler::MakeSendBuffer(Request);
	SendPacket(Buffer);
}

void UNetworkGameInstanceSubsystem::SendSavePointSet(FVector Location)
{
	se::game::C_SetSavePointReq Request;
	auto* Position = Request.mutable_position();
	Position->set_x(Location.X);
	Position->set_y(Location.Y);
	Position->set_z(Location.Z);
	
	auto Buffer = ClientPacketHandler::MakeSendBuffer(Request);
	SendPacket(Buffer);
}

void UNetworkGameInstanceSubsystem::SendWireLaunch(const FVector& StartPosition, const FVector& Direction)
{
	se::game::C_WireLaunchReq Request;
	auto* Start = Request.mutable_start_position();
	Start->set_x(StartPosition.X);
	Start->set_y(StartPosition.Y);
	Start->set_z(StartPosition.Z);

	auto* Dir = Request.mutable_direction();
	Dir->set_x(Direction.X);
	Dir->set_y(Direction.Y);
	Dir->set_z(Direction.Z);

	UE_LOG(LogTemp, Log, TEXT("[WirePkt][Stage=Send][C_WireLaunchReq] Start=(%.1f, %.1f, %.1f) Dir=(%.2f, %.2f, %.2f)"),
		StartPosition.X,
		StartPosition.Y,
		StartPosition.Z,
		Direction.X,
		Direction.Y,
		Direction.Z);

	auto Buffer = ClientPacketHandler::MakeSendBuffer(Request);
	SendPacket(Buffer);
}

void UNetworkGameInstanceSubsystem::SendItemPickUp(uint32 ItemEntityId)
{
	se::game::C_PickupItemReq Request;
	auto* ItemId = Request.mutable_item_entity_id();
	ItemId->set_value(ItemEntityId);
	
	UE_LOG(LogTemp, Log, TEXT("[ItemPkt] Item Entity Id=%u"), ItemEntityId);
	
	auto Buffer = ClientPacketHandler::MakeSendBuffer(Request);
	SendPacket(Buffer);
}

void UNetworkGameInstanceSubsystem::SendChestInteract(uint32 ChestEntityId)
{
	se::game::C_ChestInteractReq Request;
	auto* ChestId = Request.mutable_chest_entity_id();
	ChestId->set_value(ChestEntityId);
	
	UE_LOG(LogTemp, Log, TEXT("[ChestPkt] Chest Entity Id=%u"), ChestEntityId);
	auto Buffer = ClientPacketHandler::MakeSendBuffer(Request);
	SendPacket(Buffer);
}

void UNetworkGameInstanceSubsystem::ConnectToServer()
{
	Socket = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateSocket(NAME_Stream, TEXT("Client Socket"));
	if (Socket == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[Networkd] Failed to create socket"));
		return;
	}
	
	TArray<FString> HostsToTry;
	
#if WITH_EDITOR
	// 개발 빌드에서는 Fallback IP (로컬호스트)만 시도하도록 설정
	if (!ClientConfig.FallbackIp.IsEmpty())
	{
		HostsToTry.Add(ClientConfig.FallbackIp);
	}
#else
	if (!ClientConfig.ServerDNS.IsEmpty())
	{
		HostsToTry.Add(ClientConfig.ServerDNS);
	}
	if (!ClientConfig.FallbackIp.IsEmpty())
	{
		HostsToTry.Add(ClientConfig.FallbackIp);
	}
#endif

	for (const FString& Host : HostsToTry)
	{
		if (TryConnect(Host, ClientConfig.ServerPort))
		{
			UE_LOG(LogTemp, Log, TEXT("Connected to %s:%d"), *Host, ClientConfig.ServerPort);
			
			bIsConnected = true;
			PlayState = ENetworkPlayState::Connected;
		
			GameSession = MakeShared<PacketSession>(Socket);
			GameSession->Run();
			
			return;
		}
		
		UE_LOG(LogTemp, Warning, TEXT("Failed to connect to %s:%d"), *Host, ClientConfig.ServerPort);
	}
	
	HandleConnectFailed();
}

void UNetworkGameInstanceSubsystem::DisconnectFromServer()
{
	if (bDisconnecting)
		return;
	
	bDisconnecting = true;
	bIsConnected = false;
	PlayState = ENetworkPlayState::Disconnected;
	
	// 타이머 정지 (Ping 타이머)
	StopPingTimer();
	// 타이머 정지 (패킷 처리 타이머)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(QueueProcessingTimer);
	}
	
	UE_LOG(LogTemp, Log, TEXT("Disconnecting from server..."));
	
	if (Socket)
	{
		Socket->Shutdown(ESocketShutdownMode::ReadWrite);
		Socket->Close();
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
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
		Socket = nullptr;
	}
	
	ClearRoomState();
	LocalPlayerInfo = FLocalPlayerInfo();
	
	UE_LOG(LogTemp, Log, TEXT("Disconnected and cleaned up"));
}

bool UNetworkGameInstanceSubsystem::TryConnect(const FString& Host, int32 Port)
{
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (SocketSubsystem == nullptr || Socket == nullptr)
	{
		return false;
	}
	
	// IP 문자열 Try
	{
		TSharedRef<FInternetAddr> Addr = SocketSubsystem->CreateInternetAddr();

		bool bIsValidIp = false;
		Addr->SetIp(*Host, bIsValidIp);

		if (bIsValidIp)
		{
			Addr->SetPort(Port);

			UE_LOG(LogTemp, Log, TEXT("[Network] Trying direct IP connect: %s:%d"), *Host, Port);
			return Socket->Connect(*Addr);
		}
	}
	
	// DNS로 호스트명 해석 시도
	{
		FAddressInfoResult Result = SocketSubsystem->GetAddressInfo(
			*Host,
			nullptr,
			EAddressInfoFlags::Default,
			NAME_None);

		if (Result.ReturnCode != SE_NO_ERROR || Result.Results.Num() == 0)
		{
			UE_LOG(LogTemp, Error, TEXT("[Network] Failed to resolve host: %s, ReturnCode=%d"),
				*Host, (int32)Result.ReturnCode);
			return false;
		}

		// 주소들 순회하며 연결 시도
		for (const FAddressInfoResultData& Entry : Result.Results)
		{
			Entry.Address->SetPort(Port);

			UE_LOG(LogTemp, Log, TEXT("[Network] Trying resolved address %s for host %s"),
				*Entry.Address->ToString(true),
				*Host);

			if (Socket->Connect(*Entry.Address))
			{
				UE_LOG(LogTemp, Log, TEXT("[Network] Connected to %s"),
					*Entry.Address->ToString(true));
				return true;
			}
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("[Network] Failed to connect to resolved host: %s"), *Host);
	return false;
}

void UNetworkGameInstanceSubsystem::HandleConnectFailed()
{
	UE_LOG(LogTemp, Error, TEXT("Failed to connect to server"));
		
	ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
	Socket = nullptr;
	bIsConnected = false;
}

void UNetworkGameInstanceSubsystem::Handshaking()
{
	if (PlayState != ENetworkPlayState::Connected) return;
	
	se::auth::C_HandshakeReq HandshakeReq;
	HandshakeReq.set_client_protocol_version(se::protocol::kProtocolVersion);
	
	PlayState = ENetworkPlayState::Handshaking;
	
	auto Buffer = ClientPacketHandler::MakeSendBuffer(HandshakeReq);
	SendPacket(Buffer);
}

void UNetworkGameInstanceSubsystem::SpawnProcessPacketTimer()
{
	if (not bIsConnected) return;
	
	if (UWorld* World = GetWorld())
	{
		// TODO: 0.01초 값은 .ini나 .config 파일로 부터 읽어와서 적용해야 할 듯 싶다
		World->GetTimerManager().SetTimer(QueueProcessingTimer, this, &UNetworkGameInstanceSubsystem::ProcessPacket, 0.01f, true);
	}
}

void UNetworkGameInstanceSubsystem::ProcessPacket()
{
	check(IsInGameThread());
	
	if (not bIsConnected or GameSession == nullptr) return;
	
	GameSession->HandleRecvPackets();
}

bool UNetworkGameInstanceSubsystem::IsConnected() const
{
	return bIsConnected;
}

bool UNetworkGameInstanceSubsystem::CanSendGameplayPacket() const
{
	return bIsConnected && GameSession != nullptr && IsRoomPlayableState(PlayState);
}

void UNetworkGameInstanceSubsystem::HandleHandshakeRes(const se::auth::S_HandshakeRes& Pkt)
{
	check(IsInGameThread());
	
	if (!Pkt.success())
	{
		const auto& Result = Pkt.result();
		
		UE_LOG(LogTemp, Warning, TEXT("Handshake failed: %s"), UTF8_TO_TCHAR(Result.message().c_str()));
		DisconnectFromServer();
		return;
	}
	
	LocalPlayerInfo.PlayerId = Pkt.session_player_id();
	const auto& Config = Pkt.config();
	
	FRuntimeConfig NewRuntimeConfig{};
	NewRuntimeConfig.MovementUpdateHz = Config.movement_update_hz();
	NewRuntimeConfig.PingIntervalMs = Config.ping_interval_ms();
	
	if (NewRuntimeConfig.IsValid())
	{
		RuntimeConfig = NewRuntimeConfig;
	}
	
	PlayState = ENetworkPlayState::InLobby;
	
	StartPingTimer();
	
	
// TEMP (Test 용이를 위해 Connect 후 자동으로 Enter Match Queue 하도록)
#if WITH_EDITOR
	RequestMatchQueueEnter();
#endif
}

void UNetworkGameInstanceSubsystem::HandleLoginRes(const se::auth::S_LoginRes& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandlePong(const se::auth::S_Pong& Pkt)
{
	uint64 NowMs = static_cast<uint64>(FPlatformTime::Seconds() * 1000.0);
	uint64 SentTimeMs = Pkt.client_time_ms();
	// uint64 ServerTimeMs = Pkt.server_time_ms();
	
	if (NowMs < SentTimeMs)  // 시간 역전 방지
	{
		// UE_LOG(LogTemp, Warning, TEXT("[Network] HandlePong: Invalid time delta"));
		return;
	}
	
	uint64 RTT = NowMs - SentTimeMs;
	// uint64 EstimatedServerTimeMs = SentTimeMs + RTT / 2;
	
	// UE_LOG(LogTemp, Log, TEXT("[Network] Pong received. RTT = %llu ms"), RTT);
}

void UNetworkGameInstanceSubsystem::HandleSetNicknameRes(const se::lobby::S_SetNicknameRes& Pkt)
{
	check(IsInGameThread());
	
	if (!Pkt.success())
	{
		const auto& Result = Pkt.result();
		
		UE_LOG(LogTemp, Warning, TEXT("Failed to set nickname: %s"), UTF8_TO_TCHAR(Result.message().c_str()));
		return;
	}
	
	if (Pkt.nickname().empty())
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to set nickname: Empty nickname in response"));
		return;
	}
	
	LocalPlayerInfo.Nickname = UTF8_TO_TCHAR(Pkt.nickname().c_str());
	
	UE_LOG(LogTemp, Log, TEXT("Nickname set successfully: %s"), *LocalPlayerInfo.Nickname);
}

void UNetworkGameInstanceSubsystem::HandleMatchQueueEnterRes(const se::lobby::S_MatchQueueEnterRes& Pkt)
{
	check(IsInGameThread());
	
	if (!Pkt.success())
	{
		PlayState = ENetworkPlayState::InLobby;
		
		const auto& Result = Pkt.result();
		
		UE_LOG(LogTemp, Warning, TEXT("Failed to enter matchmaking queue: %s"), UTF8_TO_TCHAR(Result.message().c_str()));
		return;
	}
	
	UE_LOG(LogTemp, Log, TEXT("Entered matchmaking queue successfully"));
}

void UNetworkGameInstanceSubsystem::HandleMatchQueueCancelRes(const se::lobby::S_MatchQueueCancelRes& Pkt)
{
	check(IsInGameThread());
	
	if (!Pkt.success())
	{
		// 일단 Lobby로 돌아간다..
		PlayState = ENetworkPlayState::InLobby;
		
		const auto& Result = Pkt.result();
		
		UE_LOG(LogTemp, Warning, TEXT("Failed to cancel matchmaking queue: %s"), UTF8_TO_TCHAR(Result.message().c_str()));
		return;
	}
	
	UE_LOG(LogTemp, Log, TEXT("Cancelled matchmaking queue successfully"));
}

void UNetworkGameInstanceSubsystem::HandleMatchFound(const se::lobby::N_MatchFound& Pkt)
{
	check(IsInGameThread());
	
	uint32 RoomId = Pkt.room_id();
	if (RoomId == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Received invalid RoomId in MatchFound packet"));
		return;
	}
	
	PlayState = ENetworkPlayState::MatchingSucc;
	TryRoomId = RoomId;
	
	UE_LOG(LogTemp, Log, TEXT("Match found! RoomId=%u"), RoomId);
	
	se::room::C_RoomEnterReq RoomEnterReq;
	RoomEnterReq.set_room_id(RoomId);
	
	auto Buffer = ClientPacketHandler::MakeSendBuffer(RoomEnterReq);
	SendPacket(Buffer);
}

void UNetworkGameInstanceSubsystem::HandleRoomEnterRes(const se::room::S_RoomEnterRes& Pkt)
{
	check(IsInGameThread());
	
	if (!Pkt.success())
	{
		const auto& Result = Pkt.result();
		
		UE_LOG(LogTemp, Warning, TEXT("Failed to enter room: %s"), UTF8_TO_TCHAR(Result.message().c_str()));
		PlayState = ENetworkPlayState::InLobby;
		return;
	}
	
	if (!Pkt.has_my_entity_id())
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to enter room: Missing my_entity_id in response"));
		PlayState = ENetworkPlayState::InLobby;
		return;
	}
	
	if (!Pkt.has_snapshot())
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to enter room: Missing room snapshot in response"));
		PlayState = ENetworkPlayState::InLobby;
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
	PlayState = ENetworkPlayState::InLobby;
	
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
	uint32 EntityId = HandleSpawnInfo(Info);
	
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
	RemoveEntity(EntityId);
	
	UE_LOG(LogTemp, Log, TEXT("[Network] Entity despawned: EntityId=%u"), EntityId);
}

void UNetworkGameInstanceSubsystem::HandleRoomSetupEnd(const se::room::S_RoomSetupEnd& pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleEntitiesSpawn(const se::room::N_EntitiesSpawn& Pkt)
{
	check(IsInGameThread());
	
	if (!IsRoomPlayableState(PlayState))
	{
		return;
	}
	
	if (Pkt.infos_size() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] No room infos in room objects"));
		return;
	}
	
	for (const auto& Info : Pkt.infos())
	{
		uint32 EntityId = HandleSpawnInfo(Info);
		UE_LOG(LogTemp, Log, TEXT("[Network] Entity spawned (batch): EntityId=%u"), EntityId);
	}
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

void UNetworkGameInstanceSubsystem::HandlePlayerInitSetup(const se::game::N_PlayerInitSetup& Pkt)
{
	check(IsInGameThread());
	
	if (!IsRoomPlayableState(PlayState))
	{
		return;
	}
	
	const int MaxHealth = Pkt.max_health();
	const int CurrentHealth = Pkt.current_health();
	const int TimePoints = Pkt.time_points();
	
	ATimeThiefCharacterBase* LocalPlayer = GetLocalPlayerPawn();
	if (LocalPlayer == nullptr)
	{
		return;
	}
	
	if (auto* HealthComp = LocalPlayer->FindComponentByClass<UTimeThiefHealthComponent>())
	{
		HealthComp->SetHealth(MaxHealth, CurrentHealth);
	}
	
	if (auto* TimePointComp = LocalPlayer->FindComponentByClass<UTimePointSystemComponent>())
	{
		TimePointComp->SetTimePoints(TimePoints);
	}
	
	ATimeThiefMasterWeapon* WeaponActor = LocalPlayer->GetWeaponActor();
	
	for (const auto& WeaponInfo : Pkt.weapon_slots())
	{
		uint32 WeaponId = WeaponInfo.weapon_id();
		const auto& WeaponStat = WeaponInfo.stat();
		int MagCapacity = WeaponStat.mag_capacity();
		float FireInterval = WeaponStat.fire_interval();
		float ReloadTime = WeaponStat.reload_time();
		int32 PelletCount = WeaponStat.pellet_count();
		float ConeAngle = WeaponStat.cone_angle();
		float ProjectileSpeed = WeaponStat.projectile_speed();
		float ExplosionRadius = WeaponStat.explosion_radius();
		auto* WeaponComp = WeaponActor->GetWeaponComponentByTag(FTimeThiefGameplayTags::ResolveWeaponTagFromId(WeaponId));
		
		FWeaponStatData StatData;
		StatData.MagCapacity = MagCapacity;
		StatData.FireInterval = FireInterval;
		StatData.ReloadTime = ReloadTime;
		StatData.PelletCount = PelletCount;
		StatData.ConeAngle = ConeAngle;
		StatData.ProjectileSpeed = ProjectileSpeed;
		StatData.ExplosionRadius = ExplosionRadius;
		
		WeaponComp->SetWeaponStatForNetwork(StatData);
	}
}

void UNetworkGameInstanceSubsystem::HandleMove(const se::game::N_Move& Pkt)
{
	check(IsInGameThread());
	
	if (!IsRoomPlayableState(PlayState))
	{
		return;
	}
	
	const uint32 EntityId = Pkt.entity_id().value();
	FEntityRuntimeEntry* EntityEntry = EntityEntries.Find(EntityId);
	if (EntityEntry == nullptr)
	{
		return;
	}

	if (IsLocalPlayerEntity(EntityId))
	{
		return;
	}
	
	FNetworkEntityState& EntityState = EntityEntry->State;
	const auto& Transform = Pkt.transform();
	const auto& Position = Transform.position();
	EntityState.Position = FVector(Position.x(), Position.y(), Position.z());
	const float Yaw = Transform.yaw();
	EntityState.CharYaw = Yaw;
	
	switch (Pkt.object_type())
	{
	case se::common::ObjectType::OBJ_PLAYER:
		{
			if (!Pkt.has_player_movement())
			{
				UE_LOG(LogTemp, Warning, TEXT("[Network] HandleMove: Missing player_movement for player entity"));
				return;
			}
			
			const auto& PlayerMovement = Pkt.player_movement();
			EntityState.AimYaw = PlayerMovement.aim_yaw();
			EntityState.AimPitch = PlayerMovement.pitch();
			const auto& Velocity = PlayerMovement.velocity();
			EntityState.Velocity = FVector(Velocity.x(), Velocity.y(), 0.0f);
			EntityState.MovementMode = static_cast<EMovementMode>(PlayerMovement.movement_mode());
		}
		break;
	case se::common::ObjectType::OBJ_MONSTER:
		{
			
		}
		break;
	case se::common::ObjectType::OBJ_ITEM:
		{
			// NONE
		}
		break;
	case se::common::ObjectType::OBJ_PROJECTILE:
		{
			if (!Pkt.has_projectile_movement())
			{
				UE_LOG(LogTemp, Warning, TEXT("[Network] HandleMove: Missing projectile_movement for projectile entity"));
				return;
			}
			
			const auto& ProjectileInfo = Pkt.projectile_movement();
			const auto& Velocity = ProjectileInfo.velocity();
			EntityState.Velocity = FVector(Velocity.x(), Velocity.y(), Velocity.z());
		}
		break;
	}
	
	ApplyEntityStateToActor(EntityId);
}

void UNetworkGameInstanceSubsystem::HandleJump(const se::game::N_Jump& Pkt)
{
	check(IsInGameThread());

	if (!IsRoomPlayableState(PlayState))
	{
		return;
	}

	const uint32 EntityId = Pkt.entity_id().value();
	FEntityRuntimeEntry* EntityEntry = EntityEntries.Find(EntityId);
	if (EntityEntry == nullptr || EntityEntry->Actor == nullptr)
	{
		return;
	}

	if (IsLocalPlayerEntity(EntityId))
	{
		return;
	}

	if (auto* NMC = EntityEntry->Actor->GetComponentByClass<UNetworkMoveComponent>())
	{
		NMC->HandleActionEvent(FNetworkActionEvent{ ENetworkActionType::Jump, ENetworkActionPhase::Start });
	}
}

void UNetworkGameInstanceSubsystem::HandleJumpLand(const se::game::N_JumpLand& Pkt)
{
	check(IsInGameThread());

	if (!IsRoomPlayableState(PlayState))
	{
		return;
	}

	const uint32 EntityId = Pkt.entity_id().value();
	FEntityRuntimeEntry* EntityEntry = EntityEntries.Find(EntityId);
	if (EntityEntry == nullptr || EntityEntry->Actor == nullptr)
	{
		return;
	}

	if (IsLocalPlayerEntity(EntityId))
	{
		return;
	}

	if (auto* NMC = EntityEntry->Actor->GetComponentByClass<UNetworkMoveComponent>())
	{
		NMC->HandleActionEvent(FNetworkActionEvent{ ENetworkActionType::Jump, ENetworkActionPhase::Land });
	}
}

void UNetworkGameInstanceSubsystem::HandleDoubleJump(const se::game::N_DoubleJump& pkt)
{
	check(IsInGameThread());
	
	if (!IsRoomPlayableState(PlayState))
	{
		return;
	}
	
	const uint32 EntityId = pkt.entity_id().value();
	FEntityRuntimeEntry* EntityEntry = EntityEntries.Find(EntityId);
	if (EntityEntry == nullptr || EntityEntry->Actor == nullptr)
	{
		return;
	}
	
	if (IsLocalPlayerEntity(EntityId))
	{
		return;
	}
	
	if (auto* NMC = EntityEntry->Actor->GetComponentByClass<UNetworkMoveComponent>())
	{
		NMC->HandleActionEvent(FNetworkActionEvent{ ENetworkActionType::Jump, ENetworkActionPhase::Double });
	}
}

void UNetworkGameInstanceSubsystem::HandleCrouch(const se::game::N_Crouch& Pkt)
{
	check(IsInGameThread());
	
	if (!IsRoomPlayableState(PlayState))
	{
		return;
	}
	
	const uint32 EntityId = Pkt.entity_id().value();
	FEntityRuntimeEntry* EntityEntry = EntityEntries.Find(EntityId);
	if (EntityEntry == nullptr)
	{
		return;
	}

	if (IsLocalPlayerEntity(EntityId))
	{
		return;
	}
	
	if (auto NMC = EntityEntry->Actor->GetComponentByClass<UNetworkMoveComponent>())
	{
		ENetworkActionPhase Phase = Pkt.is_crouching() ? ENetworkActionPhase::Start : ENetworkActionPhase::End;
		NMC->HandleActionEvent(FNetworkActionEvent{ENetworkActionType::Crouch, Phase});
	}
}

void UNetworkGameInstanceSubsystem::HandleWireAction(const se::game::N_WireAction& Pkt)
{
	check(IsInGameThread());

	if (!IsRoomPlayableState(PlayState))
	{
		return;
	}

	if (!Pkt.has_entity_id() || !Pkt.has_anchor_point())
	{
		UE_LOG(LogTemp, Warning, TEXT("[WirePkt][Stage=Apply][N_WireAction] missing required fields"));
		return;
	}

	const uint32 EntityId = Pkt.entity_id().value();
	const auto& Anchor = Pkt.anchor_point();
	UE_LOG(LogTemp, Log,
		TEXT("[WirePkt][Stage=Apply][N_WireAction] EntityId=%u Anchor=(%.1f, %.1f, %.1f)"),
		EntityId,
		Anchor.x(),
		Anchor.y(),
		Anchor.z());

	if (IsLocalPlayerEntity(EntityId))
	{
		UE_LOG(LogTemp, Log, TEXT("[WirePkt][Stage=Apply][N_WireAction] skipped local entity=%u"), EntityId);
		return;
	}

	FEntityRuntimeEntry* EntityEntry = EntityEntries.Find(EntityId);
	if (EntityEntry == nullptr || EntityEntry->Actor == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WirePkt][Stage=Apply][N_WireAction] actor missing for entity=%u"), EntityId);
		return;
	}

	if (UTimeThiefWireComponent* WireComponent = EntityEntry->Actor->GetComponentByClass<UTimeThiefWireComponent>())
	{
		WireComponent->SimulateAttach(FVector(Anchor.x(), Anchor.y(), Anchor.z()));
		UE_LOG(LogTemp, Log, TEXT("[WirePkt][Stage=Apply][N_WireAction] simulate attach entity=%u actor=%s"), EntityId, *GetNameSafe(EntityEntry->Actor.Get()));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[WirePkt][Stage=Apply][N_WireAction] wire component missing entity=%u actor=%s"), EntityId, *GetNameSafe(EntityEntry->Actor.Get()));
	}
}

void UNetworkGameInstanceSubsystem::HandleWireActionEnd(const se::game::N_WireActionEnd& Pkt)
{
	check(IsInGameThread());

	if (!IsRoomPlayableState(PlayState))
	{
		return;
	}

	if (!Pkt.has_entity_id())
	{
		UE_LOG(LogTemp, Warning, TEXT("[WirePkt][Stage=Apply][N_WireActionEnd] missing entity_id"));
		return;
	}

	const uint32 EntityId = Pkt.entity_id().value();
	UE_LOG(LogTemp, Log, TEXT("[WirePkt][Stage=Apply][N_WireActionEnd] EntityId=%u"), EntityId);
	if (IsLocalPlayerEntity(EntityId))
	{
		UE_LOG(LogTemp, Log, TEXT("[WirePkt][Stage=Apply][N_WireActionEnd] skipped local entity=%u"), EntityId);
		return;
	}

	FEntityRuntimeEntry* EntityEntry = EntityEntries.Find(EntityId);
	if (EntityEntry == nullptr || EntityEntry->Actor == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WirePkt][Stage=Apply][N_WireActionEnd] actor missing for entity=%u"), EntityId);
		return;
	}

	if (UTimeThiefWireComponent* WireComponent = EntityEntry->Actor->GetComponentByClass<UTimeThiefWireComponent>())
	{
		WireComponent->SimulateDetach();
		UE_LOG(LogTemp, Log, TEXT("[WirePkt][Stage=Apply][N_WireActionEnd] simulate detach entity=%u actor=%s"), EntityId, *GetNameSafe(EntityEntry->Actor.Get()));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[WirePkt][Stage=Apply][N_WireActionEnd] wire component missing entity=%u actor=%s"), EntityId, *GetNameSafe(EntityEntry->Actor.Get()));
	}
}

void UNetworkGameInstanceSubsystem::HandleWireLaunch(const se::game::N_WireLaunch& pkt)
{
	check(IsInGameThread());

	if (!IsRoomPlayableState(PlayState))
	{
		return;
	}

	if (!pkt.has_entity_id() || !pkt.has_start_position() || !pkt.has_direction())
	{
		UE_LOG(LogTemp, Warning, TEXT("[WirePkt][Stage=Apply][N_WireLaunch] missing required fields"));
		return;
	}

	const uint32 EntityId = pkt.entity_id().value();
	const auto& Start = pkt.start_position();
	const auto& Direction = pkt.direction();
	UE_LOG(LogTemp, Log,
		TEXT("[WirePkt][Stage=Apply][N_WireLaunch] EntityId=%u Start=(%.1f, %.1f, %.1f) Dir=(%.2f, %.2f, %.2f)"),
		EntityId,
		Start.x(),
		Start.y(),
		Start.z(),
		Direction.x(),
		Direction.y(),
		Direction.z());

	if (IsLocalPlayerEntity(EntityId))
	{
		UE_LOG(LogTemp, Log, TEXT("[WirePkt][Stage=Apply][N_WireLaunch] skipped local entity=%u"), EntityId);
		return;
	}

	FEntityRuntimeEntry* EntityEntry = EntityEntries.Find(EntityId);
	if (EntityEntry == nullptr || EntityEntry->Actor == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WirePkt][Stage=Apply][N_WireLaunch] actor missing for entity=%u"), EntityId);
		return;
	}

	if (UTimeThiefWireComponent* WireComponent = EntityEntry->Actor->GetComponentByClass<UTimeThiefWireComponent>())
	{
		WireComponent->SimulateLaunch(FVector(Start.x(), Start.y(), Start.z()), FVector(Direction.x(), Direction.y(), Direction.z()));
		UE_LOG(LogTemp, Log, TEXT("[WirePkt][Stage=Apply][N_WireLaunch] simulate launch entity=%u actor=%s"), EntityId, *GetNameSafe(EntityEntry->Actor.Get()));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[WirePkt][Stage=Apply][N_WireLaunch] wire component missing entity=%u actor=%s"), EntityId, *GetNameSafe(EntityEntry->Actor.Get()));
	}
}

void UNetworkGameInstanceSubsystem::HandleAim(const se::game::N_Aim& Pkt)
{
	check(IsInGameThread());
	
	if (!IsRoomPlayableState(PlayState))
	{
		return;
	}
	
	const uint32 EntityId = Pkt.entity_id().value();
	const FEntityRuntimeEntry* Entry = EntityEntries.Find(EntityId);
	FRemoteAttackNotify Notify{};
	Notify.AttackerEntityId = EntityId;
	Notify.NotifyType = Pkt.is_aiming()
		? ECombatNotifyType::Aiming
		: ECombatNotifyType::Readying;

	if (Entry)
	{
		const FRotator AimRotation = UTimeThiefAimStatics::BuildAimRotation(Entry->State.AimPitch, Entry->State.CharYaw);
		Notify.Direction = UTimeThiefAimStatics::ResolveAimDirectionFromRotation(AimRotation);
		Notify.Origin = Entry->State.Position;
	}
	else
	{
		Notify.Direction = FVector::ZeroVector;
		Notify.Origin = FVector::ZeroVector;
	}
	
	ApplyRemoteAttackNotifyToActor(EntityId, Notify);
}

void UNetworkGameInstanceSubsystem::HandleFire(const se::game::N_Fire& Pkt)
{
	check(IsInGameThread());
	
	if (!IsRoomPlayableState(PlayState))
	{
		return;
	}
	
	const uint32 EntityId = Pkt.entity_id().value();
	FRemoteAttackNotify Notify{};
	Notify.AttackerEntityId = EntityId;
	Notify.NotifyType = ECombatNotifyType::Fire;
	Notify.WeaponId = Pkt.weapon_id();
	const auto& Origin = Pkt.start_position();
	Notify.Origin = FVector(Origin.x(), Origin.y(), Origin.z());
	const auto& Dir = Pkt.direction();
	Notify.Direction = FVector(Dir.x(), Dir.y(), Dir.z());
	Notify.ShotSeed = Pkt.shot_seed();
	
	ApplyRemoteAttackNotifyToActor(EntityId, Notify);
}

void UNetworkGameInstanceSubsystem::HandleAttack(const se::game::N_Attack& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleThrowGrenade(const se::game::N_ThrowGrenade& Pkt)
{
	check(IsInGameThread());
	
	if (!IsRoomPlayableState(PlayState))
	{
		return;
	}
	
	const uint32 EntityId = Pkt.entity_id().value();
	FRemoteAttackNotify Notify{};
	Notify.AttackerEntityId = EntityId;
	Notify.NotifyType = ECombatNotifyType::Throw;
	Notify.WeaponId = Pkt.grenade_type();
	const auto& Origin = Pkt.start_position();
	Notify.Origin = FVector(Origin.x(), Origin.y(), Origin.z());
	const auto& Dir = Pkt.direction();
	Notify.Direction = FVector(Dir.x(), Dir.y(), Dir.z());
	
	ApplyRemoteAttackNotifyToActor(EntityId, Notify);
}

void UNetworkGameInstanceSubsystem::HandleReload(const se::game::N_Reload& Pkt)
{
	check(IsInGameThread());
	
	if (!IsRoomPlayableState(PlayState))
	{
		return;
	}
	
	const uint32 EntityId = Pkt.entity_id().value();
	FRemoteAttackNotify Notify{};
	Notify.AttackerEntityId = EntityId;
	Notify.NotifyType = ECombatNotifyType::Reload;
	Notify.WeaponId = Pkt.weapon_id();
	
	ApplyRemoteAttackNotifyToActor(EntityId, Notify);
}

void UNetworkGameInstanceSubsystem::HandleWeaponChanged(const se::game::N_WeaponChanged& Pkt)
{
	check(IsInGameThread());
	
	if (!IsRoomPlayableState(PlayState))
	{
		return;
	}
	
	const uint32 EntityId = Pkt.entity_id().value();
	FRemoteAttackNotify Notify{};
	Notify.AttackerEntityId = EntityId;
	Notify.NotifyType = ECombatNotifyType::WeaponChange;
	Notify.WeaponId = Pkt.weapon_id();
	
	ApplyRemoteAttackNotifyToActor(EntityId, Notify);
}

void UNetworkGameInstanceSubsystem::HandleWeaponStatSnapshot(const se::game::N_WeaponStatSnapshot& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleUseAbility(const se::game::N_UseAbility& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleKillPlayer(const se::game::N_KillPlayer& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleReloadRes(const se::game::S_ReloadRes& pkt)
{
	check(IsInGameThread());
	
	if (!IsRoomPlayableState(PlayState))
	{
		return;
	}
	
	const bool bSuccess = pkt.success();
	if (!bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to reload (From Server)"));
		return;
	}
	
	const uint32 WeaponId = pkt.weapon_id();
	const uint32 ReloadedAmmo = pkt.reloaded_ammo();	// Delta Ammo
	const uint32 RemainingAmmo = pkt.remaining_ammo();	// 현재 장전된 탄약
	
	if (auto LocalPlayerPawn = GetLocalPlayerPawn())
	{
		if (auto CombatComp = LocalPlayerPawn->FindComponentByClass<UTimeThiefPlayerCombatComponent>())
		{
			if (auto MasterWeapon = CombatComp->GetMasterWeapon())
			{
				if (auto EquipWeapon = MasterWeapon->GetActiveWeaponComponent())
				{
					if (EquipWeapon->GetWeaponTag() != FTimeThiefGameplayTags::ResolveWeaponTagFromId(WeaponId))
					{
						// 재장전 완료 패킷이 왔지만 다른 무기로 장착이 바뀐 경우, 재장전 결과를 무시한다. <- 재장전 실패 한 것
						UE_LOG(LogTemp, Warning, TEXT("Received ReloadRes for WeaponId=%u, but currently equipped weapon is different. Ignoring reload result."), WeaponId);
						return;
					}
					
					EquipWeapon->HandleReloadResult(ReloadedAmmo, RemainingAmmo);
				}
			}
		}
	}
}

void UNetworkGameInstanceSubsystem::HandleEntityHit(const se::game::N_EntityHit& pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleWeaponStatChanged(const se::game::N_WeaponStatChanged& pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleUseItem(const se::game::N_UseItem& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleSetSavePointRes(const se::game::S_SetSavePointRes& pkt)
{
	if (!pkt.success())
	{
		const auto& Result = pkt.result();
		
		UE_LOG(LogTemp, Warning, TEXT("Failed to set save point: %s"), UTF8_TO_TCHAR(Result.message().c_str()));
		return;
	}
	
	const auto& Pos = pkt.position();
	FVector SavePoint(Pos.x(), Pos.y(), Pos.z());
	
	auto LocalPlayerPawn = GetLocalPlayerPawn();
	if (LocalPlayerPawn == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to set save point: Local player pawn not found"));
		return;
	}
	
	if (auto SaveSkill = LocalPlayerPawn->GetSavePointSkillComponent())
	{
		if (SaveSkill->CanActivate())
		{
			SaveSkill->ActivateSkill();
			UE_LOG(LogTemp, Log, TEXT("Save point set successfully"));
			
			return;
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("Save Skill not ready(or not found), can't set save point"));
}

void UNetworkGameInstanceSubsystem::HandlePickupItem(const se::game::N_PickupItem& Pkt)
{
	// A 플레이어가 아이템 먹음
	UE_LOG(LogTemp, Log, TEXT("PickupItem received"));
	const uint32 PlayerID = Pkt.entity_id().value();
	const uint32 ItemID = Pkt.item_entity_id().value();
	
	UE_LOG(LogTemp, Log, TEXT("Entity %u picked up item %u"), PlayerID, ItemID);
	
	auto ItemEntry = EntityEntries.Find(ItemID);
	if (ItemEntry == nullptr || !ItemEntry->Actor.IsValid())
	{
		return;
	}
	if (auto ItemActor = Cast<AItemBase>(ItemEntry->Actor))
	{
		ItemActor->Disable();
	}
}

void UNetworkGameInstanceSubsystem::HandleUseStoreRes(const se::game::S_UseStoreRes& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleItemGained(const se::game::N_ItemGained& Pkt)
{
	// 실제로 아이템이 늘어나는 곳
	// 저 패킷에 뭐뭐 있는지
	// 
}

void UNetworkGameInstanceSubsystem::HandleChestInteracted(const se::game::N_ChestInteracted& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleItemLost(const se::game::N_ItemLost& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleItemSnapshot(const se::game::N_ItemSnapshot& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleEquipItem(const se::game::N_EquipItem& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleEquipItemRes(const se::game::S_EquipItemRes& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleUseItemRes(const se::game::S_UseItemRes& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleHealthChanged(const se::game::N_HealthChanged& Pkt)
{
	check(IsInGameThread());
	
	if (!IsRoomPlayableState(PlayState))
	{
		return;
	}
	
	const uint32 EntityId = Pkt.entity_id().value();
	if (EntityId == LocalPlayerEntityId)
	{
		// 로컬 플레이어의 체력 변동만 유효함
		auto Entry = EntityEntries.Find(EntityId);
		if (Entry == nullptr)
		{
			return;
		}
		
		auto Actor = Entry->Actor.Get();
		if (Actor == nullptr)
		{
			return;
		}
		
		auto HealthComp = Actor->FindComponentByClass<UTimeThiefHealthComponent>();
		if (HealthComp == nullptr)
		{
			return;
		}
		
		HealthComp->HandleHealthChanged(Pkt.new_health(), Pkt.delta());
	}
	else
	{
		// Remote Player나 Server Auth NPC 객체의 체력이 회복되는 이펙트
		// 체력이 줄어드는 이펙트가 필요하다면 여기서 연결
	}
}

void UNetworkGameInstanceSubsystem::HandleEntityDied(const se::game::N_EntityDied& Pkt)
{
	check(IsInGameThread());
	
	if (!IsRoomPlayableState(PlayState))
	{
		return;
	}
	
	const uint32 EntityId = Pkt.entity_id().value();
	auto Entry = EntityEntries.Find(EntityId);
	if (Entry == nullptr || Entry->Actor == nullptr)
	{
		return;
	}
	
	auto Actor = Entry->Actor.Get();
	if (Actor == nullptr)
	{
		return;
	}
	
	auto TTCharacter = dynamic_cast<ATimeThiefCharacterBase*>(Actor);
	if (TTCharacter == nullptr)
	{
		return;
	}
	
	if (EntityId == LocalPlayerEntityId)
	{
		// 로컬 플레이어 사망 처리 후 컨트롤 불가하게
		TTCharacter->HandleDeathFromServer();
	}
	else
	{
		// 타 플레이어는 사망 연출 진행
		TTCharacter->HandleDeathFromServer();
	}
}

void UNetworkGameInstanceSubsystem::HandleEntityRespawned(const se::game::N_EntityRespawned& Pkt)
{
	check(IsInGameThread());
	
	if (!IsRoomPlayableState(PlayState))
	{
		return;
	}
	
	const uint32 EntityId = Pkt.entity_id().value();
	const auto& Transform = Pkt.transform();
	const auto& Pos = Transform.position();
	FVector RespawnPosition(Pos.x(), Pos.y(), Pos.z());
	const float Yaw = Transform.yaw();
	auto Entry = EntityEntries.Find(EntityId);
	if (Entry == nullptr || Entry->Actor == nullptr)
	{
		return;
	}
	
	auto Actor = Entry->Actor.Get();
	if (Actor == nullptr)
	{
		return;
	}
	
	auto TTCharacter = dynamic_cast<ATimeThiefCharacterBase*>(Actor);
	if (TTCharacter == nullptr)
	{
		return;
	}
	
	if (EntityId == LocalPlayerEntityId)
	{
		// 로컬 플레이어 부활 처리 후 컨트롤 가능하게
		TTCharacter->HandleRespawnFromServer(RespawnPosition);
	}
	else
	{
		// 타 플레이어는 부활 연출 진행
		TTCharacter->HandleRespawnFromServer(RespawnPosition);
	}
}

void UNetworkGameInstanceSubsystem::HandleEntityDestroyed(const se::game::N_EntityDestroyed& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleTimePointChanged(const se::game::N_TimePointChanged& Pkt)
{
	check(IsInGameThread());
	
	if (!IsRoomPlayableState(PlayState))
	{
		return;
	}
	
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	
	ATimeThiefCharacterBase* LocalPlayer = GetLocalPlayerPawn();
	UTimePointSystemComponent* TimePointComp = LocalPlayer ? LocalPlayer->FindComponentByClass<UTimePointSystemComponent>() : nullptr;
	if (TimePointComp == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] Local player has no UTimePointSystemComponent"));
		return;
	}
	
	TimePointComp->UpdateTimePoints(Pkt.new_time_points(), Pkt.delta());
}

void UNetworkGameInstanceSubsystem::HandleTimePointSnapshot(const se::game::N_TimePointSnapshot& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleSkillUnlock(const se::game::N_SkillUnlock& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleSkillEquipRes(const se::game::S_SkillEquipRes& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleSkillUnlockSnapshot(const se::game::N_SkillUnlockSnapshot& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleMaxHealthChanged(const se::game::N_MaxHealthChanged& Pkt)
{
	check(IsInGameThread());
	
	if (!IsRoomPlayableState(PlayState))
	{
		return;
	}
	
	const uint32 EntityId = Pkt.entity_id().value();
	if (EntityId == LocalPlayerEntityId)
	{
		// 로컬 플레이어의 체력 변동만 유효함
		auto Entry = EntityEntries.Find(EntityId);
		if (Entry == nullptr)
		{
			return;
		}
		
		auto Actor = Entry->Actor.Get();
		if (Actor == nullptr)
		{
			return;
		}
		
		auto HealthComp = Actor->FindComponentByClass<UTimeThiefHealthComponent>();
		if (HealthComp == nullptr)
		{
			return;
		}
		
		HealthComp->SetHealth(Pkt.new_max_health(), Pkt.new_current_health());
	}
}

void UNetworkGameInstanceSubsystem::HandleHealthSnapshot(const se::game::N_HealthSnapshot& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleTimeStormChange(const se::game::N_TimeStormChange& Pkt)
{
	check(IsInGameThread());
	
	// GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, FString::Printf(TEXT("Time Storm Changed: Radus=%.1f, Wait=%.1f, Shrink=%.1f"), Pkt.radius(), Pkt.waiting_time(), Pkt.shrinking_time()));
	
	if (!IsRoomPlayableState(PlayState))
	{
		return;
	}
	
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	
	AGameStateBase* GameState = World->GetGameState();
	if (GameState == nullptr)
	{
		return;
	}
	
	UTimeStormComponent* TimeStormComp = GameState->FindComponentByClass<UTimeStormComponent>();
	if (TimeStormComp == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] GameState has no UTimeStormComponent"));
		return;
	}
	
	auto Pos = Pkt.center();
	const FVector2D DestCenter(Pos.x(), Pos.y());
	const float DestRadius = Pkt.radius();
	
	TimeStormComp->SetStormPhase(DestCenter, DestRadius, Pkt.waiting_time(), Pkt.shrinking_time());
}

void UNetworkGameInstanceSubsystem::HandleZoneStop(const se::test::N_ZoneStop& Pkt)
{
	check(IsInGameThread());
	
	if (!IsRoomPlayableState(PlayState))
	{
		return;
	}
	
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	
	AGameStateBase* GameState = World->GetGameState();
	if (GameState == nullptr)
	{
		return;
	}
	
	UTimeStormComponent* TimeStormComp = GameState->FindComponentByClass<UTimeStormComponent>();
	if (TimeStormComp == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] GameState has no UTimeStormComponent"));
		return;
	}
	
	// 자기장 멈췄기에 클라이언트에서도 멈추어야 함
	TimeStormComp->ZoneFlow(false);
}

void UNetworkGameInstanceSubsystem::HandleZoneStart(const se::test::N_ZoneStart& Pkt)
{
	check(IsInGameThread());
	
	if (!IsRoomPlayableState(PlayState))
	{
		return;
	}
	
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	
	AGameStateBase* GameState = World->GetGameState();
	if (GameState == nullptr)
	{
		return;
	}
	
	UTimeStormComponent* TimeStormComp = GameState->FindComponentByClass<UTimeStormComponent>();
	if (TimeStormComp == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] GameState has no UTimeStormComponent"));
		return;
	}
	
	// 자기장 시작했기에 클라이언트에서도 시작해야 함
	TimeStormComp->ZoneFlow(true);
}

uint32 UNetworkGameInstanceSubsystem::HandleSpawnInfo(const se::room::SpawnInfo& Info)
{
	const uint32 EntityId = Info.entity_id().value();
	
	FEntityRuntimeEntry& EntityEntry = EntityEntries.FindOrAdd(EntityId);
	EntityEntry.EntityId = EntityId;
	FNetworkEntityState& EntityState = EntityEntry.State;
	EntityState.EntityId = EntityId;
	
	EntityState.ObjectType = Info.type();
	EntityState.TemplateId = Info.template_id();

	switch (Info.type())
	{
	case se::common::ObjectType::OBJ_PLAYER:
		{
			if (!Info.has_player_info())
			{
				UE_LOG(LogTemp, Warning, TEXT("[Network] HandleEntitySpawn: Missing player_info for player entity"));
				return 0;
			}
			
			const auto& PlayerInfo = Info.player_info();
			
			const auto& Movement = PlayerInfo.movement();
			const auto& Pos = Movement.position();
			EntityState.Position = FVector(Pos.x(), Pos.y(), Pos.z());
			EntityState.CharYaw = Movement.yaw();
			EntityState.AimPitch = Movement.pitch();
		}
		break;
	case se::common::ObjectType::OBJ_MONSTER:
		{
			
		}
		break;
	case se::common::ObjectType::OBJ_ITEM:
		{
			if (!Info.has_item_info())
			{
				UE_LOG(LogTemp, Warning, TEXT("[Network] HandleEntitySpawn: Missing item_info for item entity"));
				return 0;
			}
			
			const auto& ItemInfo = Info.item_info();
			const auto& Pos = ItemInfo.position();
			EntityState.Position = FVector(Pos.x(), Pos.y(), Pos.z());
			const auto& Velocity = ItemInfo.velocity();
			EntityState.Velocity = FVector(Velocity.x(), Velocity.y(), Velocity.z());
			EntityState.ItemCount = ItemInfo.amount();
		}
		break;
	case se::common::ObjectType::OBJ_PROJECTILE:
		{
			if (!Info.has_projectile_info())
			{
				UE_LOG(LogTemp, Warning, TEXT("[Network] HandleEntitySpawn: Missing projectile_info for projectile entity"));
				return 0;
			}
			
			const auto& ProjectileInfo = Info.projectile_info();
			const auto& Pos = ProjectileInfo.position();
			EntityState.Position = FVector(Pos.x(), Pos.y(), Pos.z());
			const auto& Velocity = ProjectileInfo.velocity();
			EntityState.Velocity = FVector(Velocity.x(), Velocity.y(), Velocity.z());
		}
		break;
	case se::common::ObjectType::OBJ_CHEST:
		{
			if (!Info.has_chest_info())
			{
				UE_LOG(LogTemp, Warning, TEXT("[Network] HandleEntitySpawn: Missing chest_info for chest entity"));
				return 0;
			}
			
			const auto& ChestInfo = Info.chest_info();
			const auto& Pos = ChestInfo.position();
			EntityState.Position = FVector(Pos.x(), Pos.y(), Pos.z());
			const float Yaw = ChestInfo.yaw();
			EntityState.CharYaw = Yaw;
		}
		break;
	case se::common::ObjectType::OBJ_STORE:
		{
			if (!Info.has_store_info())
			{
				UE_LOG(LogTemp, Warning, TEXT("[Network] HandleEntitySpawn: Missing store_info for store entity"));
				return 0;
			}
			
			const auto& StoreInfo = Info.store_info();
			const auto& Pos = StoreInfo.position();
			EntityState.Position = FVector(Pos.x(), Pos.y(), Pos.z());
			const float Yaw = StoreInfo.yaw();
			EntityState.CharYaw = Yaw;
		}
		break;
	}
	
	ApplyEntityStateToActor(EntityId);
	return EntityId;
}

void UNetworkGameInstanceSubsystem::RemoveEntity(uint32 EntityId)
{
	if (EntityId == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] RemoveEntity failed: Invalid EntityId"));
		return;
	}

	FEntityRuntimeEntry* EntityEntry = EntityEntries.Find(EntityId);
	if (EntityEntry == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] RemoveEntity failed: Entity entry not found for EntityId %u"), EntityId);
		return;
	}

	if (EntityEntry->Actor.IsValid())
	{
		AActor* Actor = EntityEntry->Actor.Get();
		if (Actor != nullptr)
		{
			UE_LOG(LogTemp, Log, TEXT("[Network] RemoveEntity: Destroy Actor. EntityId=%u, Actor=%s"), EntityId, *Actor->GetName());
			Actor->Destroy();
		}

		EntityEntry->Actor.Reset();
	}

	EntityEntries.Remove(EntityId);

	if (EntityId == LocalPlayerEntityId)
	{
		LocalPlayerEntityId = 0;
	}

	UE_LOG(LogTemp, Log, TEXT("[Network] RemoveEntity success: EntityId=%u"), EntityId);
}

bool UNetworkGameInstanceSubsystem::IsLocalPlayerEntity(uint32 EntityId) const
{
	return LocalPlayerEntityId != 0 && LocalPlayerEntityId == EntityId;
}

TSubclassOf<AActor> UNetworkGameInstanceSubsystem::ResolveActorClass(const FNetworkEntityState& EntityState) const
{
	// TODO: EntityState의 정보를 바탕으로 어떤 Actor 클래스를 스폰할지 결정하는 로직을 구현해야 한다
	//		 어떤 ObjectType, TemplateId 여도 처리할 수 있도록 (조합가능한 기준)
	if (!SpawnData) return nullptr;

	if (EntityState.ObjectType == se::common::OBJ_PLAYER)
	{
		if (SpawnData->LocalPlayerClass)
		{
			return SpawnData->LocalPlayerClass;
		}
	}
	else if (EntityState.ObjectType == se::common::OBJ_PROJECTILE)
	{
		// if (SpawnData->RocketProjectileClass)
		// {
		// 	return SpawnData->RocketProjectileClass;
		// }
	}
	else if (EntityState.ObjectType == se::common::OBJ_CHEST)
	{
		if (SpawnData->ChestClass)
		{
			return SpawnData->ChestClass;
		}
	}
	else if (EntityState.ObjectType == se::common::OBJ_ITEM)
	{
		if (SpawnData->ItemClass)
		{
			return SpawnData->ItemClass;
		}
	}
	
	const int32 ObjectTypeValue = static_cast<int32>(EntityState.ObjectType);
	
	if (const TSubclassOf<AActor>* Found = SpawnData->SpawnClassMap.Find(ObjectTypeValue))
	{
		return *Found;
	}
	
	return nullptr;
}

bool UNetworkGameInstanceSubsystem::LoadClientConfig()
{
	const FString FilePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("RuntimeConfig/client.dev.json"));
	
	UE_LOG(LogTemp, Log, TEXT("[Config] Loading client config from %s"), *FilePath);
	
	return FClientConfigLoader::LoadClientConfigFromFile(FilePath, ClientConfig);
}

ATimeThiefPlayerCharacter* UNetworkGameInstanceSubsystem::GetLocalPlayerPawn()
{
	if (GEngine == nullptr || GetWorld() == nullptr)
	{
		return nullptr;
	}
	
	if (LocalPlayerEntityId == 0)
		return nullptr;
	
	FEntityRuntimeEntry* EntityEntry = EntityEntries.Find(LocalPlayerEntityId);
	if (EntityEntry == nullptr || EntityEntry->Actor == nullptr)
	{
		return nullptr;
	}
	
	auto Actor = EntityEntry->Actor.Get();
	if (Actor == nullptr)
	{
		return nullptr;
	}
	
	return Cast<ATimeThiefPlayerCharacter>(Actor);
}

void UNetworkGameInstanceSubsystem::RequestSetNickname(const FString& Nickname)
{
	if (bIsConnected == false || GameSession == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] Cannot request to set nickname: Not connected to server"));
		return;
	}
	
	if (PlayState != ENetworkPlayState::InLobby)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] Cannot request to set nickname: Invalid state"));
		return;
	}
	
	se::lobby::C_SetNicknameReq Request;
	Request.set_nickname(TCHAR_TO_UTF8(*Nickname));
	
	auto SendBuffer = ClientPacketHandler::MakeSendBuffer(Request);
	SendPacket(SendBuffer);
	
	UE_LOG(LogTemp, Log, TEXT("[Network] Sent C_SetNicknameReq to server. Nickname=%s"), *Nickname);
}

void UNetworkGameInstanceSubsystem::RequestMatchQueueEnter()
{
	if (bIsConnected == false || GameSession == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] Cannot request to enter match queue: Not connected to server"));
		return;
	}
	
	if (PlayState != ENetworkPlayState::InLobby)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] Cannot request to enter match queue: Invalid state"));
		return;
	}
	
	se::lobby::C_MatchQueueEnterReq Request;
	
	auto SendBuffer = ClientPacketHandler::MakeSendBuffer(Request);
	SendPacket(SendBuffer);
	
	PlayState = ENetworkPlayState::MatchMaking;
	
	UE_LOG(LogTemp, Log, TEXT("[Network] Sent C_MatchQueueEnterReq to server"));
}

void UNetworkGameInstanceSubsystem::RequestMatchQueueCancel()
{
	if (bIsConnected == false || GameSession == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] Cannot request to cancel match queue: Not connected to server"));
		return;
	}
	
	if (PlayState != ENetworkPlayState::MatchMaking)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] Cannot request to cancel match queue: Invalid state"));
		return;
	}
	
	se::lobby::C_MatchQueueCancelReq Request;
	
	auto SendBuffer = ClientPacketHandler::MakeSendBuffer(Request);
	SendPacket(SendBuffer);
	
	UE_LOG(LogTemp, Log, TEXT("[Network] Sent C_MatchQueueCancelReq to server"));	
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

void UNetworkGameInstanceSubsystem::RequestSpawnMonster(FVector Pos, uint32 MonsterType)
{
	if (bIsConnected == false || GameSession == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] Cannot request loading complete: Not connected to server"));
		return;
	}
	
	se::test::C_SpawnMonsterReq Request;
	Request.set_enemy_type(MonsterType);
	auto* SpawnPos = Request.mutable_spawn_position();
	SpawnPos->set_x(Pos.X);
	SpawnPos->set_y(Pos.Y);
	SpawnPos->set_z(Pos.Z);
	
	auto SendBuffer = ClientPacketHandler::MakeSendBuffer(Request);
	SendPacket(SendBuffer);
	
	UE_LOG(LogTemp, Log, TEXT("[Network] Sent C_SpawnMonsterReq to server. MonsterType=%u, Pos=(%.1f, %.1f, %.1f)"), MonsterType, Pos.X, Pos.Y, Pos.Z);
}

void UNetworkGameInstanceSubsystem::RequestSpawnChest(FVector Pos)
{
	if (bIsConnected == false || GameSession == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] Cannot request to spawn chest: Not connected to server"));
		return;
	}
	
	se::test::C_SpawnChestReq Request;
	auto* SpawnPos = Request.mutable_spawn_position();
	SpawnPos->set_x(Pos.X);
	SpawnPos->set_y(Pos.Y);
	SpawnPos->set_z(Pos.Z);
	
	auto SendBuffer = ClientPacketHandler::MakeSendBuffer(Request);
	SendPacket(SendBuffer);
	
	UE_LOG(LogTemp, Log, TEXT("[Network] Sent C_SpawnChestReq to server. Pos=(%.1f, %.1f, %.1f)"), Pos.X, Pos.Y, Pos.Z);
}

void UNetworkGameInstanceSubsystem::RequestSpawnStore(FVector Pos)
{
	if (bIsConnected == false || GameSession == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] Cannot request to spawn store: Not connected to server"));
		return;
	}
	
	se::test::C_SpawnStoreReq Request;
	auto* SpawnPos = Request.mutable_spawn_position();
	SpawnPos->set_x(Pos.X);
	SpawnPos->set_y(Pos.Y);
	SpawnPos->set_z(Pos.Z);
	
	auto SendBuffer = ClientPacketHandler::MakeSendBuffer(Request);
	SendPacket(SendBuffer);
	
	UE_LOG(LogTemp, Log, TEXT("[Network] Sent C_SpawnStoreReq to server. Pos=(%.1f, %.1f, %.1f)"), Pos.X, Pos.Y, Pos.Z);
}

void UNetworkGameInstanceSubsystem::RequestItemReq(uint32 ItemId, int32 Amount)
{
	if (bIsConnected == false || GameSession == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] Cannot request item: Not connected to server"));
		return;
	}
	
	se::test::C_ItemReq Request;
	Request.set_item_id(ItemId);
	Request.set_quantity(Amount);
	
	auto SendBuffer = ClientPacketHandler::MakeSendBuffer(Request);
	SendPacket(SendBuffer);
	
	UE_LOG(LogTemp, Log, TEXT("[Network] Sent C_ItemReq to server. ItemId=%u, Amount=%d"), ItemId, Amount);
}

void UNetworkGameInstanceSubsystem::RequestMoneyReq(int32 Amount)
{
	if (bIsConnected == false || GameSession == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] Cannot request money: Not connected to server"));
		return;
	}
	
	se::test::C_MoneyReq Request;
	Request.set_amount(Amount);
	
	auto SendBuffer = ClientPacketHandler::MakeSendBuffer(Request);
	SendPacket(SendBuffer);
	
	UE_LOG(LogTemp, Log, TEXT("[Network] Sent C_MoneyReq to server. Amount=%d"), Amount);
}

void UNetworkGameInstanceSubsystem::RequestHealthReq(int32 Health)
{
	if (bIsConnected == false || GameSession == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] Cannot request health: Not connected to server"));
		return;
	}
	
	se::test::C_HealthReq Request;
	Request.set_health(Health);
	
	auto SendBuffer = ClientPacketHandler::MakeSendBuffer(Request);
	SendPacket(SendBuffer);
	
	UE_LOG(LogTemp, Log, TEXT("[Network] Sent C_HealthReq to server. Health=%d"), Health);
}

void UNetworkGameInstanceSubsystem::RequestMaxHealthReq(int32 MaxHealth)
{
	if (bIsConnected == false || GameSession == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] Cannot request max health: Not connected to server"));
		return;
	}
	
	se::test::C_MaxHealthReq Request;
	Request.set_max_health(MaxHealth);
	
	auto SendBuffer = ClientPacketHandler::MakeSendBuffer(Request);
	SendPacket(SendBuffer);
	
	UE_LOG(LogTemp, Log, TEXT("[Network] Sent C_MaxHealthReq to server. MaxHealth=%d"), MaxHealth);
}

void UNetworkGameInstanceSubsystem::RequestZoneStop()
{
	if (bIsConnected == false || GameSession == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] Cannot request to stop zone: Not connected to server"));
		return;
	}
	
	se::test::C_ZoneStopReq Request;
	
	auto SendBuffer = ClientPacketHandler::MakeSendBuffer(Request);
	SendPacket(SendBuffer);
	
	UE_LOG(LogTemp, Log, TEXT("[Network] Sent C_ZoneStopReq to server"));
}

void UNetworkGameInstanceSubsystem::RequestZoneStart()
{
	if (bIsConnected == false || GameSession == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] Cannot request to start zone: Not connected to server"));
		return;
	}
	
	se::test::C_ZoneStartReq Request;
	
	auto SendBuffer = ClientPacketHandler::MakeSendBuffer(Request);
	SendPacket(SendBuffer);
	
	UE_LOG(LogTemp, Log, TEXT("[Network] Sent C_ZoneStartReq to server"));
}

void UNetworkGameInstanceSubsystem::RequestZoneReset()
{
	if (bIsConnected == false || GameSession == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] Cannot request to reset zone: Not connected to server"));
		return;
	}
	
	se::test::C_ZoneResetReq Request;
	
	auto SendBuffer = ClientPacketHandler::MakeSendBuffer(Request);
	SendPacket(SendBuffer);
	
	UE_LOG(LogTemp, Log, TEXT("[Network] Sent C_ZoneResetReq to server"));
}

void UNetworkGameInstanceSubsystem::RequestZoneDamageOff()
{
	if (bIsConnected == false || GameSession == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] Cannot request to turn off zone damage: Not connected to server"));
		return;
	}
	
	se::test::C_ZoneDamageOffReq Request;
	
	auto SendBuffer = ClientPacketHandler::MakeSendBuffer(Request);
	SendPacket(SendBuffer);
	
	UE_LOG(LogTemp, Log, TEXT("[Network] Sent C_ZoneDamageOffReq to server"));
}

void UNetworkGameInstanceSubsystem::RequestZoneDamageOn()
{
	if (bIsConnected == false || GameSession == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] Cannot request to turn on zone damage: Not connected to server"));
		return;
	}
	
	se::test::C_ZoneDamageOnReq Request;
	
	auto SendBuffer = ClientPacketHandler::MakeSendBuffer(Request);
	SendPacket(SendBuffer);
	
	UE_LOG(LogTemp, Log, TEXT("[Network] Sent C_ZoneDamageOnReq to server"));
}

void UNetworkGameInstanceSubsystem::Ping()
{
	if (bIsConnected == false || GameSession == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] Cannot send ping: Not connected to server"));
		return;
	}
	
	se::auth::C_Ping Request;
	
	uint64 NowMs = static_cast<uint64>(FPlatformTime::Seconds() * 1000.0);
	Request.set_client_time_ms(NowMs);
	
	auto SendBuffer = ClientPacketHandler::MakeSendBuffer(Request);
	if (!SendBuffer)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] Cannot send ping: Failed to create send buffer"));
		return;
	}
	
	SendPacket(SendBuffer);
}

void UNetworkGameInstanceSubsystem::StartPingTimer()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] StartPingTimer failed: World is null"));
		return;
	}
	
	if (RuntimeConfig.PingIntervalMs <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] StartPingTimer failed: Invalid PingIntervalMs = %d"), RuntimeConfig.PingIntervalMs);
		return;
	}
	
	const float PingIntervalSeconds = RuntimeConfig.GetPingIntervalSeconds();
	
	World->GetTimerManager().ClearTimer(PingTimer);
	World->GetTimerManager().SetTimer(PingTimer, this, &UNetworkGameInstanceSubsystem::Ping, PingIntervalSeconds, true);
	
	UE_LOG(LogTemp, Log, TEXT("[Network] Ping timer started. Interval = %.3f sec"), PingIntervalSeconds);
}

void UNetworkGameInstanceSubsystem::StopPingTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PingTimer);
	}

	UE_LOG(LogTemp, Log, TEXT("[Network] Ping timer stopped"));
}

void UNetworkGameInstanceSubsystem::ClearRoomState()
{
	for (auto& Pair : EntityEntries)
	{
		if (Pair.Value.Actor.IsValid())
		{
			Pair.Value.Actor->Destroy();
			Pair.Value.Actor.Reset();
		}
	}

	EntityEntries.Empty();
	LocalPlayerEntityId = 0;
	RoomState = FRoomState();
}

AActor* UNetworkGameInstanceSubsystem::FindEntityActor(uint32 EntityId) const
{
	const FEntityRuntimeEntry* EntityEntry = EntityEntries.Find(EntityId);
	if (EntityEntry == nullptr) return nullptr;
	
	return EntityEntry->Actor.Get();
}

AActor* UNetworkGameInstanceSubsystem::SpawnEntityActor(const FNetworkEntityState& EntityState)
{
	UWorld* World = GetWorld();
	if (World == nullptr) return nullptr;
	
	FEntityRuntimeEntry* EntityEntry = EntityEntries.Find(EntityState.EntityId);
	if (EntityEntry == nullptr) return nullptr;
	
	TSubclassOf<AActor> ActorClass = ResolveActorClass(EntityState);
	if (ActorClass == nullptr) return nullptr;
	
	const FRotator SpawnRotation(0.0f, EntityState.CharYaw, 0.0f);
	const FTransform SpawnTransform(SpawnRotation, EntityState.Position);
	
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	
	// TODO: World가 GameWorld가 아니면 안된다 (문제가 생긴 것)
	AActor* SpawnedActor;
	if (EntityState.ObjectType == se::common::OBJ_ITEM)
	{
		// 내꺼
		// ItemID = EntityState.TemplateId
		const TSubclassOf<AActor> ItemClass = GetDefault<UItemSettings>()->GetItemClass(EntityState.TemplateId);
		SpawnedActor = GetWorld()->GetSubsystem<UItemPoolWorldSubsystem>()->Get(ItemClass);
		
		if (SpawnedActor == nullptr)
		{
			return nullptr;
		}
		
		SpawnedActor->SetActorTransform(SpawnTransform);
	}
	else
	{
		SpawnedActor = World->SpawnActor<AActor>(ActorClass, SpawnTransform, SpawnParams);
	}
	if (SpawnedActor == nullptr) return nullptr;

	
	EntityEntry->Actor = SpawnedActor;
	
	PostSpawnEntityActor(SpawnedActor, EntityState);
	
	return SpawnedActor;
}

AActor* UNetworkGameInstanceSubsystem::GetOrSpawnEntityActor(uint32 EntityId)
{
	if (AActor* ExistingActor = FindEntityActor(EntityId))
	{
		return ExistingActor;
	}
	
	const FEntityRuntimeEntry* EntityEntry = EntityEntries.Find(EntityId);
	if (EntityEntry == nullptr) return nullptr;
	
	return SpawnEntityActor(EntityEntry->State);
}

void UNetworkGameInstanceSubsystem::InitializeSpawnedPawnData(AActor* Actor)
{
	ATimeThiefPlayerCharacter* PlayerCharacter = Cast<ATimeThiefPlayerCharacter>(Actor);
	if (PlayerCharacter == nullptr)
	{
		return;
	}
	
	const UTimeThiefPawnData* PawnData = GetDefaultPawnData();
	if (PawnData == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] InitializeSpawnedPawnData: DefaultPawnData is null on %s"),
			*GetNameSafe(PlayerCharacter));
		return;
	}
	
	PlayerCharacter->SetPawnData(PawnData);
	
	UE_LOG(LogTemp, Log, TEXT("[Network] InitializeSpawnedPawnData: Set PawnData=%s on %s"),
		*GetNameSafe(PawnData),
		*GetNameSafe(PlayerCharacter));
}

void UNetworkGameInstanceSubsystem::ApplySpawnRuntimeStateToActor(AActor* Actor, const FNetworkEntityState& EntityState)
{
	if (auto NetEntity = Cast<INetworkEntityInterface>(Actor))
	{
		NetEntity->ApplySpawnRuntimeState(EntityState);
	}
	switch (EntityState.ObjectType)
	{
	case se::common::ObjectType::OBJ_PROJECTILE:
		{
			if (ATimeThiefRocketProjectile* Projectile = Cast<ATimeThiefRocketProjectile>(Actor))
			{
				ATimeThiefCharacterBase* LocalPlayer = GetLocalPlayerPawn();
				if (LocalPlayer == nullptr)
				{
					UE_LOG(LogTemp, Warning, TEXT("[Network] ApplySpawnRuntimeStateToActor: Local player pawn is null"));
					return;
				}
				ATimeThiefMasterWeapon* WeaponActor = LocalPlayer->GetWeaponActor();
				if (WeaponActor == nullptr)
				{
					UE_LOG(LogTemp, Warning, TEXT("[Network] ApplySpawnRuntimeStateToActor: Local player weapon actor is null"));
					return;
				}
				auto* WeaponComp = WeaponActor->GetWeaponComponentByTag(FTimeThiefGameplayTags::Get().Weapon_RocketLauncher);
				if (WeaponComp == nullptr)
				{
					UE_LOG(LogTemp, Warning, TEXT("[Network] ApplySpawnRuntimeStateToActor: Weapon actor has no matching weapon component"));
					return;
				}
				auto* LauncherComp = Cast<UTimeThiefRocketLauncherComponent>(WeaponComp);
				if (LauncherComp == nullptr)
				{
					UE_LOG(LogTemp, Warning, TEXT("[Network] ApplySpawnRuntimeStateToActor: Weapon component is not UTimeThiefRocketLauncherComponent"));
					return;
				}
				
				Projectile->InitializeProjectileSettings(LauncherComp->GetProjectileSpeed(), LauncherComp->GetExplosionRadius());
				Projectile->InitializeProjectile(LocalPlayer, LocalPlayer);
				Projectile->ActivateProjectileFromNetwork(EntityState.Position, EntityState.Velocity);
			}
		}
	}

}

void UNetworkGameInstanceSubsystem::PostSpawnEntityActor(AActor* SpawnedActor, const FNetworkEntityState& EntityState)
{
	if (SpawnedActor == nullptr) return;
	
	InitializeNetworkEntityActor(SpawnedActor, EntityState);;
	InitializeSpawnedPawnData(SpawnedActor);
	ApplyRuntimeConfigToActor(SpawnedActor);
	
	ApplySpawnRuntimeStateToActor(SpawnedActor, EntityState);
	
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
		NetworkEntityComp->SetControlType(ENetworkControlType::ServerAuth);
		break;
	}
}

void UNetworkGameInstanceSubsystem::ApplyRuntimeConfigToActor(AActor* Actor)
{
	if (Actor == nullptr)
	{
		return;
	}
	
	UNetworkMoveComponent* MoveComp = Actor->FindComponentByClass<UNetworkMoveComponent>();
	if (MoveComp == nullptr)
	{
		return;
	}
	
	if (!RuntimeConfig.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] RuntimeConfig invalid, skip applying to actor: %s"), *GetNameSafe(Actor));
		return;
	}
	
	MoveComp->SetMovementUpdateInterval(RuntimeConfig.GetMovementUpdateIntervalSeconds());
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
	const FEntityRuntimeEntry* EntityEntry = EntityEntries.Find(EntityId);
	if (EntityEntry == nullptr) return;
	
	AActor* Actor = GetOrSpawnEntityActor(EntityId);
	if (Actor == nullptr) return;
	
	ApplyEntityStateToActor(Actor, EntityEntry->State);
}

void UNetworkGameInstanceSubsystem::ApplyEntityStateToActor(AActor* Actor, const FNetworkEntityState& EntityState)
{
	if (Actor == nullptr) return;
	
	if (IMovableNetworkEntityInterface* Movable = Cast<IMovableNetworkEntityInterface>(Actor))
	{
		Movable->ApplyNetworkMovementState(EntityState);
		return;
	}
	
	if (ATimeThiefRocketProjectile* Projectile = Cast<ATimeThiefRocketProjectile>(Actor))
	{
		Projectile->ApplyNetworkMovementState(EntityState);
		return;
	}
	
	const FRotator NewRotation{0.0f, EntityState.CharYaw, 0.0f};
	Actor->SetActorLocation(EntityState.Position);
	Actor->SetActorRotation(NewRotation);
}

void UNetworkGameInstanceSubsystem::ApplyAllEntityStates()
{
	for (const TPair<uint32, FEntityRuntimeEntry>& Pair : EntityEntries)
	{
		const uint32 EntityId = Pair.Key;
		ApplyEntityStateToActor(EntityId);
	}
}

void UNetworkGameInstanceSubsystem::ApplyRemoteAttackNotifyToActor(uint32 EntityId, const FRemoteAttackNotify& Notify)
{
	if (IsLocalPlayerEntity(EntityId))
	{
		return;
	}

	AActor* Actor = FindEntityActor(EntityId);
	if (Actor == nullptr)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[Network] ApplyRemoteAttackNotifyToActor failed: Actor not found. EntityId=%u"), EntityId);
		return;
	}
	
	ICombatSyncInterface* CombatSync = Cast<ICombatSyncInterface>(Actor);
	if (CombatSync == nullptr)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[Network] ApplyRemoteAttackNotifyToActor skipped: Actor does not implement CombatSyncInterface. Actor=%s"),
			*GetNameSafe(Actor));
		return;
	}

	UNetworkCombatSyncComponent* CombatSyncComponent = CombatSync->GetCombatSyncComponent();
	if (CombatSyncComponent == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] ApplyRemoteAttackNotifyToActor failed: CombatSyncComponent is null. Actor=%s"),
			*GetNameSafe(Actor));
		return;
	}

	CombatSyncComponent->BroadcastRemoteAttackNotify(Notify);
}
