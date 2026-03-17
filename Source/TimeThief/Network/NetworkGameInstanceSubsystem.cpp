// Fill out your copyright notice in the Description page of Project Settings.
#include "Network/NetworkGameInstanceSubsystem.h"

#include <Generated/ClientPacketHandler.h>

#include "SocketSubsystem.h"
#include "Interfaces/IPv4/IPv4Address.h"

#include "PacketSession.h"
#include "ClientConfigLoader.h"
#include "TestPlayer/NTLocalPlayer.h"

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

// void UNetworkGameInstanceSubsystem::HandleSpawn(const se::room::N_EntitySpawn& SpawnPkt)
// {
// 	if (Socket == nullptr or GameSession == nullptr) return;
// 	
// 	const auto& ObjectType = SpawnPkt.entity_type();
// 	const auto& Entity = SpawnPkt.entity();
// 	
// 	SpawnEntity(ObjectType, Entity);
// }

bool UNetworkGameInstanceSubsystem::LoadClientConfig()
{
	const FString FilePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("../External/ProtocolShared/Config/client.dev.json"));
	
	UE_LOG(LogTemp, Log, TEXT("[Config] Loading client config from %s"), *FilePath);
	
	return FClientConfigLoader::LoadClientConfigFromFile(FilePath, ClientConfig);
}

void UNetworkGameInstanceSubsystem::ApplyEntityStateToActor(uint32 EntityId)
{
	FNetworkEntityState* State = NetworkEntities.Find(EntityId);
	if (!State) return;
	
	TWeakObjectPtr<AActor>* ActorPtr = EntityActors.Find(EntityId);
	if (!ActorPtr) return;
	if (!ActorPtr->IsValid()) return;
	
	AActor* Actor = ActorPtr->Get();
	if (!Actor) return;
	
	FRotator NewRotation(0.0f, State->Yaw, 0.0f);
	
	if (ANTPlayer* Player = Cast<ANTPlayer>(Actor))
	{
		Player->SetDestPosition(State->Position);
		Player->SetTargetYaw(State->Yaw);
		Player->SetTargetPitch(State->Pitch);
	}
	else
	{
		Actor->SetActorLocation(State->Position);
		Actor->SetActorRotation(NewRotation);
	}
	
	// TODO: 좀더 다른 방향으로 State를 전달해야 한다
	// 임시로 NTPlayer에 함수를 추가하고 해당 객체로 캐스팅하여 함수 호출 하는 방식
	// 다음에는 interface 등 더 우아한 방법으로 교체하도록
}
