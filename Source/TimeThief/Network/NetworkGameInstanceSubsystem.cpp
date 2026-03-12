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
	
	FIPv4Address ip;
	FIPv4Address::Parse(IPAddress, ip);
	
	TSharedPtr<FInternetAddr> addr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
	addr->SetIp(ip.Value);
	addr->SetPort(Port);

	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("Connecting to Server...")));
	
	bool connected = Socket->Connect(*addr);
	
	if (connected)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, FString::Printf(TEXT("Connection Success")));
		
		GameSession = MakeShared<PacketSession>(Socket);
		GameSession->Run();
		
		se::lobby::C_LobbyEnterReq lobbyEnterReq;
		auto packet = ClientPacketHandler::MakeSendBuffer(lobbyEnterReq);
		SendPacket(packet);
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("Connection Failed")));
	}
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

void UNetworkGameInstanceSubsystem::HandleLobbyEnter(const se::lobby::S_LobbyEnterRes& LobbyEnterPkt)
{
	if (Socket == nullptr or GameSession == nullptr) return;
	
	auto* World = GetWorld();
	if (World == nullptr) return;
	
	const se::common::Result& Result = LobbyEnterPkt.result();
	if (Result.code() != se::common::ERR_NONE) 
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to enter lobby: %s"), UTF8_TO_TCHAR(Result.message().c_str()));
		return;
	}
	
	const auto& PlayerInfo = LobbyEnterPkt.profile();
	PlayerId = PlayerInfo.player_id().value();
	PlayerName = UTF8_TO_TCHAR(PlayerInfo.nickname().c_str());
	Level = PlayerInfo.level();
}

void UNetworkGameInstanceSubsystem::HandleSpawn(const se::room::N_EntitySpawn& SpawnPkt)
{
	if (Socket == nullptr or GameSession == nullptr) return;
	
	const auto& ObjectType = SpawnPkt.entity_type();
	const auto& Entity = SpawnPkt.entity();
	
	SpawnEntity(ObjectType, Entity);
}

void UNetworkGameInstanceSubsystem::HandleMove(const se::room::S_EntityState& EntityStatePkt)
{
	if (Socket == nullptr or GameSession == nullptr) return;
	
	auto* World = GetWorld();
	if (World == nullptr) return;
	
	for (const auto& Entity : EntityStatePkt.entities())
	{
		const uint32 EntityId = Entity.entity_id().value();
		
		FNetworkEntityState& State = NetworkEntities.FindOrAdd(EntityId);
		const auto& newPos = Entity.movement().position();
		State.Position = FVector(newPos.x(), newPos.y(), newPos.z());
		const float yaw = Entity.movement().yaw();
		State.Yaw = yaw;
		const float pitch = Entity.movement().pitch();
		State.Pitch = pitch;
		
		ApplyEntityStateToActor(EntityId);
	}
}

void UNetworkGameInstanceSubsystem::SpawnEntity(const se::common::ObjectType& ObjectType,
	const se::room::EntityState& EntityState)
{
	auto* World = GetWorld();
	if (World == nullptr) return;
	
	// TODO: ObjectType에 따라서 다른 Actor 클래스를 Spawn해야 할 듯 싶다 (예: Player, NPC, Item 등)
	AActor* SpawnActor = World->SpawnActor<AActor>();
	if (not SpawnActor) return;
	
	const auto& Movement = EntityState.movement();
	const uint32 EntityId = EntityState.entity_id().value();
	EntityActors.Add(EntityId, SpawnActor);
	
	FNetworkEntityState& State = NetworkEntities.FindOrAdd(EntityId);
	const auto& newPos = Movement.position();
	State.Position = FVector(newPos.x(), newPos.y(), newPos.z());
	const float yaw = Movement.yaw();
	State.Yaw = yaw;
	const float pitch = Movement.pitch();
	State.Pitch = pitch;
	
	ApplyEntityStateToActor(EntityId);
}

bool UNetworkGameInstanceSubsystem::LoadClientConfig()
{
	const FString FilePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("../External/ProtocolShared/Config/client.dev.json"));
	
	UE_LOG(LogTemp, Log, TEXT("[Config] Loading client config from %s"), *FilePath);
	
	return FClientConfigLoader::LoadClientConfigFromFile(FilePath, ClientConfig);
}

void UNetworkGameInstanceSubsystem::ApplyEntityStateToActor(uint32 EntityId)
{
	FNetworkEntityState* State = NetworkEntities.Find(EntityId);
	if (not State) return;
	
	TWeakObjectPtr<AActor>* ActorPtr = EntityActors.Find(EntityId);
	if (not ActorPtr or not ActorPtr->IsValid()) return;
	
	AActor* Actor = ActorPtr->Get();
	if (not Actor) return;
	
	// Actor->SetActorLocation(State->Position);
	// Actor->SetActorRotation(State->Rotation);
	
	// TODO: 좀더 다른 방향으로 State를 전달해야 한다
	// 임시로 NTPlayer에 함수를 추가하고 해당 객체로 캐스팅하여 함수 호출 하는 방식
	// 다음에는 interface 등 더 우아한 방법으로 교체하도록
}
