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
#include "Actors/ChestActor.h"
#include "Actors/Item/ItemBase.h"
#include "Actors/StoreActor.h"
#include "Character/TimeThiefPlayerCharacter.h"
#include "Character/TimeThiefPlayerController.h"
#include "Components/TimeThiefHealthComponent.h"
#include "Components/Combat/TimeThiefPlayerCombatComponent.h"
#include "Components/Skill/SkillBaseComponent.h"
#include "Components/System/InventorySystemComponent.h"
#include "Components/System/TimePointSystemComponent.h"
#include "Components/System/TimeStormComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "DrawDebugHelpers.h"
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
#include "Monster/TimeThiefMonster.h"
#include "Utils/TimeThiefAimStatics.h"
#include "Weapon/TimeThiefMasterWeapon.h"
#include "Weapon/TimeThiefRocketProjectile.h"
#include "Weapon/TimeThiefThrowableProjectile.h"
#include "Weapon/Components/ThrowableNetworkSyncComponent.h"
#include "Weapon/Components/TimeThiefRocketLauncherComponent.h"
#include "Weapon/Components/TimeThiefWeaponComponentBase.h"

namespace
{
	static bool IsRoomPlayableState(ENetworkPlayState State)
	{
		return State == ENetworkPlayState::InRoom;
	}

	static bool UsesSpawnTransformOnly(se::common::ObjectType ObjectType)
	{
		switch (ObjectType)
		{
		case se::common::ObjectType::OBJ_CHEST:
		case se::common::ObjectType::OBJ_STORE:
			return true;
		default:
			return false;
		}
	}

	static FVector ToVector(const se::common::Vector3& Vector)
	{
		return FVector(Vector.x(), Vector.y(), Vector.z());
	}

	static void FillProtoVector(se::common::Vector3* OutVector, const FVector& InVector)
	{
		if (OutVector == nullptr)
		{
			return;
		}

		OutVector->set_x(InVector.X);
		OutVector->set_y(InVector.Y);
		OutVector->set_z(InVector.Z);
	}

	static FRotator ToRotator(const se::common::Rotator& Rotator)
	{
		return FRotator(Rotator.pitch(), Rotator.yaw(), Rotator.roll());
	}

	static FColor ToDebugDrawColor(uint32 ColorRgba)
	{
		if (ColorRgba == 0)
		{
			return FColor::Green;
		}

		return FColor(
			static_cast<uint8>((ColorRgba >> 24) & 0xFF),
			static_cast<uint8>((ColorRgba >> 16) & 0xFF),
			static_cast<uint8>((ColorRgba >> 8) & 0xFF),
			static_cast<uint8>(ColorRgba & 0xFF));
	}

	static float ToDebugDrawDuration(float Duration)
	{
		return Duration > 0.0f ? Duration : 1.0f;
	}

	static float ToDebugDrawThickness(float Thickness)
	{
		return Thickness > 0.0f ? Thickness : 1.0f;
	}

	static constexpr int32 EntitySpawnBatchSize = 10;
	static constexpr float EntitySpawnBatchIntervalSeconds = 0.01f;
	static constexpr int32 DebugDrawSphereSegments = 24;
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

void UNetworkGameInstanceSubsystem::SetPlayState(ENetworkPlayState NewState)
{
	if (PlayState == NewState)
	{
		return;
	}

	PlayState = NewState;
	OnPlayStateChanged.Broadcast(NewState);
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

void UNetworkGameInstanceSubsystem::SendStoreUse(uint32 StoreEntityId, uint32 ItemId)
{
	se::game::C_UseStoreReq Request;
	auto* StoreId = Request.mutable_store_entity_id();
	StoreId->set_value(StoreEntityId);
	Request.set_store_item_id(ItemId);
	
	UE_LOG(LogTemp, Log, TEXT("[StorePkt] Store Entity Id=%u, Item Id=%u"), StoreEntityId, ItemId);
	
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

void UNetworkGameInstanceSubsystem::SendUseItem(uint32 Itemid)
{
	se::game::C_UseItemReq Request;
	Request.set_item_id(Itemid);
	
	UE_LOG(LogTemp, Log, TEXT("[ItemPkt] Use Item Id=%u"), Itemid);
	auto Buffer = ClientPacketHandler::MakeSendBuffer(Request);
	SendPacket(Buffer);
}

void UNetworkGameInstanceSubsystem::SendUseSkill(uint32 SlotIndex, uint32 SkillId, const FUseSkillRequestDetail& Detail)
{
	// Minsoo
	// 사용 방법
	// SendUseSkill( [스킬 장착 슬롯 index 0, 1 중 하나], [스킬 아이디 1, 2, 3 중 하나], [스킬 별 추가 정보] );
	// 위와 같은 식으로 호출
	// 예시1) SendUseSkill(0, 2, FUseSkillRequestDetail::MakeAfterImage(FVector(100, 200, 300), FVector(1, 0, 0)));
	// 예시2) SendUseSkill(1, 3, FUseSkillRequestDetail::MakeRewind(3000, FVector(400, 500, 600)));
	// 예시3) SendUseSkill(0, 1); // 추가 정보 없는 스킬 (시간 가속 사용)
	// 스킬 Id는 
	// 1: 시간 가속 (추가 정보 없음)
	// 2: 시간 잔상 (추가 정보: 시작 위치, 방향)
	// 3: 시간 역행 (추가 정보: 역행 시간, 예측된 목표 위치) 역행 시간은 3000ms로 일단 고정 (3초 전의 Pos를 예측된 목표 위치로 같이 보내줘야 함)
	
	se::game::C_UseSkillReq Request;
	Request.set_slot_index(SlotIndex);
	Request.set_skill_id(SkillId);

	switch (Detail.Type)
	{
	case EUseSkillRequestDetailType::AfterImage:
		{
			auto* AfterImage = Request.mutable_after_image();
			FillProtoVector(AfterImage->mutable_start_position(), Detail.StartPosition);
			FillProtoVector(AfterImage->mutable_direction(), Detail.Direction);
			break;
		}
	case EUseSkillRequestDetailType::Rewind:
		{
			auto* Rewind = Request.mutable_rewind();
			Rewind->set_rewind_duration_ms(Detail.RewindDurationMs);
			FillProtoVector(Rewind->mutable_predicted_target_position(), Detail.PredictedTargetPosition);
			break;
		}
	case EUseSkillRequestDetailType::None:
	default:
		break;
	}

	UE_LOG(LogTemp, Log, TEXT("[SkillPkt] Use Skill Slot=%u, SkillId=%u, Detail=%d"),
		SlotIndex,
		SkillId,
		static_cast<int32>(Detail.Type));
	auto Buffer = ClientPacketHandler::MakeSendBuffer(Request);
	SendPacket(Buffer);
}

void UNetworkGameInstanceSubsystem::SendSkillEquip(uint32 SlotIndex, uint32 SkillId)
{
	se::game::C_SkillEquipReq Request;
	Request.set_slot_index(SlotIndex);
	Request.set_skill_id(SkillId);

	UE_LOG(LogTemp, Log, TEXT("[SkillPkt] Equip Skill Slot=%u, SkillId=%u"), SlotIndex, SkillId);
	auto Buffer = ClientPacketHandler::MakeSendBuffer(Request);
	SendPacket(Buffer);
}

void UNetworkGameInstanceSubsystem::SendGrenadeMoveSync(const FThrowableMoveSnapshot& MoveData)
{
	se::game::C_GrenadeMoveSyncReq Request;
	auto* ObjectIdPtr = Request.mutable_entity_id();
	ObjectIdPtr->set_value(MoveData.ObjectId);
	auto* PositionPtr = Request.mutable_position();
	PositionPtr->set_x(MoveData.Location.X);
	PositionPtr->set_y(MoveData.Location.Y);
	PositionPtr->set_z(MoveData.Location.Z);
	auto* RotationPtr = Request.mutable_rotation();
	RotationPtr->set_yaw(MoveData.Rotation.Yaw);
	RotationPtr->set_pitch(MoveData.Rotation.Pitch);
	RotationPtr->set_roll(MoveData.Rotation.Roll);
	auto* VelocityPtr = Request.mutable_velocity();
	VelocityPtr->set_x(MoveData.Velocity.X);
	VelocityPtr->set_y(MoveData.Velocity.Y);
	VelocityPtr->set_z(MoveData.Velocity.Z);

	// 빈번한 패킷이므로 로그 미출력
	// UE_LOG(LogTemp, Log, TEXT("[GrenadeMoveSync] ObjectId=%u Pos=(%.1f, %.1f, %.1f) Vel=(%.2f, %.2f, %.2f)"),
	// 	MoveData.ObjectId,
	// 	MoveData.Location.X,
	// 	MoveData.Location.Y,
	// 	MoveData.Location.Z,
	// 	MoveData.Velocity.X,
	// 	MoveData.Velocity.Y,
	// 	MoveData.Velocity.Z);
	auto Buffer = ClientPacketHandler::MakeSendBuffer(Request);
	SendPacket(Buffer);
}

void UNetworkGameInstanceSubsystem::SendGrenadeExplosion(uint32 GrenadeEntityId, const FVector& Location)
{
	se::game::C_GrenadeExplosionReq Request;
	auto* GrenadeIdPtr = Request.mutable_entity_id();
	GrenadeIdPtr->set_value(GrenadeEntityId);
	auto* PositionPtr = Request.mutable_position();
	PositionPtr->set_x(Location.X);
	PositionPtr->set_y(Location.Y);
	PositionPtr->set_z(Location.Z);
	
	// UE_LOG(LogTemp, Log, TEXT("[GrenadeExplosion] GrenadeEntityId=%u Pos=(%.1f, %.1f, %.1f)"),
	// 	GrenadeEntityId,
	// 	Location.X,
	// 	Location.Y,
	// 	Location.Z);
	
	auto Buffer = ClientPacketHandler::MakeSendBuffer(Request);
	SendPacket(Buffer);
}

bool UNetworkGameInstanceSubsystem::GetStoreItemPrice(uint32 StoreItemId, int32& OutPrice) const
{
	if (const int32* FoundPrice = StoreItemPrices.Find(StoreItemId))
	{
		OutPrice = *FoundPrice;
		return true;
	}

	return false;
}

bool UNetworkGameInstanceSubsystem::IsStoreItemSoldOut(uint32 StoreItemId) const
{
	return SoldOutStoreItems.Contains(StoreItemId);
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
			SetPlayState(ENetworkPlayState::Connected);
		
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
	SetPlayState(ENetworkPlayState::Disconnected);
	
	// 타이머 정지 (Ping 타이머)
	StopPingTimer();
	// 타이머 정지 (패킷 처리 타이머)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(QueueProcessingTimer);
		World->GetTimerManager().ClearTimer(EntitySpawnProcessingTimer);
		World->GetTimerManager().ClearTimer(PlayerInitSetupRetryTimer);
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

	SetPlayState(ENetworkPlayState::Handshaking);
	
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

	SetPlayState(ENetworkPlayState::InLobby);

	StartPingTimer();
	
	
// TEMP (Test 용이를 위해 Connect 후 자동으로 Enter Match Queue 하도록)
#if WITH_EDITOR
	// RequestMatchQueueEnter();
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
		SetPlayState(ENetworkPlayState::InLobby);

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
		SetPlayState(ENetworkPlayState::InLobby);
		
		const auto& Result = Pkt.result();
		
		UE_LOG(LogTemp, Warning, TEXT("Failed to cancel matchmaking queue: %s"), UTF8_TO_TCHAR(Result.message().c_str()));
		return;
	}
	
	SetPlayState(ENetworkPlayState::InLobby);
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
	
	SetPlayState(ENetworkPlayState::MatchingSucc);
	TryRoomId = RoomId;
	
	ResetLoadingGate();
	ResetTimeStormState();
	
	SetLocalPlayerInputEnabled(false);
	
	se::room::C_RoomEnterReq RoomEnterReq;
	RoomEnterReq.set_room_id(RoomId);
	
	auto Buffer = ClientPacketHandler::MakeSendBuffer(RoomEnterReq);
	SendPacket(Buffer);
	
	SetPlayState(ENetworkPlayState::EnteringRoom);
	UE_LOG(LogTemp, Log, TEXT("Match found! RoomId=%u"), RoomId);
}

void UNetworkGameInstanceSubsystem::HandleRoomEnterRes(const se::room::S_RoomEnterRes& Pkt)
{
	check(IsInGameThread());
	
	if (!Pkt.success())
	{
		const auto& Result = Pkt.result();
		
		UE_LOG(LogTemp, Warning, TEXT("Failed to enter room: %s"), UTF8_TO_TCHAR(Result.message().c_str()));
		SetPlayState(ENetworkPlayState::InLobby);
		return;
	}

	if (!Pkt.has_my_entity_id())
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to enter room: Missing my_entity_id in response"));
		SetPlayState(ENetworkPlayState::InLobby);
		return;
	}

	if (!Pkt.has_snapshot())
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to enter room: Missing room snapshot in response"));
		SetPlayState(ENetworkPlayState::InLobby);
		SetLocalPlayerInputEnabled(true);
		return;
	}
	
	ClearRoomState();
	
	const auto& Snapshot = Pkt.snapshot();
	
	LocalPlayerEntityId = Pkt.my_entity_id().value();
	RoomState.RoomId = Snapshot.room_id();
	bRoomStateCleared = false;
	
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
	
	bReceivedRoomEnterRes = true;
	SetPlayState(ENetworkPlayState::LoadingRoom);

	UE_LOG(LogTemp, Log, TEXT("[Network] Room enter success. RoomId=%u, LocalEntityId=%u"), RoomState.RoomId, LocalPlayerEntityId);
	// RequestLoadingComplete();	// TEMP
	TrySendLoadingComplete();
}

void UNetworkGameInstanceSubsystem::HandleRoomLeaveRes(const se::room::S_RoomLeaveRes& Pkt)
{
	check(IsInGameThread());
	
	if (!Pkt.success())
	{
		const auto& Result = Pkt.result();
		
		UE_LOG(LogTemp, Warning, TEXT("Failed to leave room: %s"), UTF8_TO_TCHAR(Result.message().c_str()));
		SetPlayState(ENetworkPlayState::InRoom);
		return;
	}
	
	if (IsRoomStateCleared())
	{
		SetPlayState(ENetworkPlayState::InLobby);
		UE_LOG(LogTemp, Log, TEXT("[Network] Room leave success ignored: room state already cleared"));
		return;
	}

	ClearRoomState();
	SetPlayState(ENetworkPlayState::InLobby);
	
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

void UNetworkGameInstanceSubsystem::HandleRoomSetupEnd(const se::room::S_RoomSetupEnd& Pkt)
{
	// TODO: Room Setup이 끝났다는 패킷이 오면 Server의 Room Setting에 관한 요청이 모두 종료 된 것
	//		 이에 대해 처리를 다 완수 하였으면 게임 시작 준비가 되었음을 서버에 알려야 함
}

void UNetworkGameInstanceSubsystem::HandleEntitiesSpawn(const se::room::N_EntitiesSpawn& Pkt)
{
	check(IsInGameThread());
	
	if (Pkt.infos_size() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] No room infos in room objects"));
		bReceivedEntitiesSpawn = true;
		TryApplyPendingPlayerInitSetup();
		TrySendLoadingComplete();
		return;
	}
	
	StartPendingEntitySpawn(Pkt);
}

void UNetworkGameInstanceSubsystem::StartPendingEntitySpawn(const se::room::N_EntitiesSpawn& Pkt)
{
	CancelPendingEntitySpawn();
	RemoveEntitiesByObjectType(se::common::OBJ_STORE);

	PendingEntitySpawnInfos.Reserve(Pkt.infos_size());
	for (const auto& Info : Pkt.infos())
	{
		PendingEntitySpawnInfos.Add(Info);
	}

	bReceivedEntitiesSpawn = false;
	PendingEntitySpawnIndex = 0;

	UE_LOG(LogTemp, Log, TEXT("[Network] Entity batch spawn queued. Count=%d"), PendingEntitySpawnInfos.Num());

	ProcessPendingEntitySpawn();

	if (PendingEntitySpawnInfos.Num() > 0)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				EntitySpawnProcessingTimer,
				this,
				&UNetworkGameInstanceSubsystem::ProcessPendingEntitySpawn,
				EntitySpawnBatchIntervalSeconds,
				true);
		}
	}
}

void UNetworkGameInstanceSubsystem::ProcessPendingEntitySpawn()
{
	check(IsInGameThread());

	if (PendingEntitySpawnIndex >= PendingEntitySpawnInfos.Num())
	{
		FinishPendingEntitySpawn();
		return;
	}

	const int32 EndIndex = FMath::Min(PendingEntitySpawnIndex + EntitySpawnBatchSize, PendingEntitySpawnInfos.Num());
	for (; PendingEntitySpawnIndex < EndIndex; ++PendingEntitySpawnIndex)
	{
		HandleSpawnInfo(PendingEntitySpawnInfos[PendingEntitySpawnIndex]);
	}

	if (PendingEntitySpawnIndex >= PendingEntitySpawnInfos.Num())
	{
		FinishPendingEntitySpawn();
	}
}

void UNetworkGameInstanceSubsystem::FinishPendingEntitySpawn()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EntitySpawnProcessingTimer);
	}

	const int32 SpawnCount = PendingEntitySpawnInfos.Num();
	PendingEntitySpawnInfos.Empty();
	PendingEntitySpawnIndex = 0;

	bReceivedEntitiesSpawn = true;
	UE_LOG(LogTemp, Log, TEXT("[Network] Entity batch spawn complete. Count=%d"), SpawnCount);

	TryApplyPendingPlayerInitSetup();
	TrySendLoadingComplete();
}

void UNetworkGameInstanceSubsystem::CancelPendingEntitySpawn()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EntitySpawnProcessingTimer);
	}

	PendingEntitySpawnInfos.Empty();
	PendingEntitySpawnIndex = 0;
}

void UNetworkGameInstanceSubsystem::HandleRoomClosed(const se::room::N_RoomClosed& Pkt)
{
	check(IsInGameThread());
	
	if (IsRoomStateCleared())
	{
		SetPlayState(ENetworkPlayState::InLobby);
		UE_LOG(LogTemp, Log, TEXT("[Network] Room close ignored: room state already cleared. RoomId=%u"), Pkt.room_id());
		return;
	}
	
	ClearRoomState();
	SetPlayState(ENetworkPlayState::InLobby);
	
	UE_LOG(LogTemp, Log, TEXT("[Network] Room close. RoomId=%u"), Pkt.room_id());
}

void UNetworkGameInstanceSubsystem::HandleGameStart(const se::game::N_GameStart& Pkt)
{
	check(IsInGameThread());

	SetPlayState(ENetworkPlayState::InRoom);

	SetLocalPlayerInputEnabled(true);

	UE_LOG(LogTemp, Log, TEXT("[Network] Game started"));
}

void UNetworkGameInstanceSubsystem::HandleGameEnd(const se::game::N_GameEnd& Pkt)
{
	// TODO: Game End 시 종료에 대한 처리
}

void UNetworkGameInstanceSubsystem::HandlePlayerInitSetup(const se::game::N_PlayerInitSetup& Pkt)
{
	check(IsInGameThread());

	if (!ApplyPlayerInitSetup(Pkt))
	{
		PendingPlayerInitSetup = Pkt;
		bHasPendingPlayerInitSetup = true;
		UE_LOG(LogTemp, Log, TEXT("[Network] Player init setup deferred until local player pawn is ready"));
		return;
	}
	
	bReceivedPlayerInitSetup = true;
	bHasPendingPlayerInitSetup = false;
	TrySendLoadingComplete();
}

bool UNetworkGameInstanceSubsystem::ApplyPlayerInitSetup(const se::game::N_PlayerInitSetup& Pkt)
{
	const int MaxHealth = Pkt.max_health();
	const int CurrentHealth = Pkt.current_health();
	const int TimePoints = Pkt.time_points();
	const float MoveSpeed = Pkt.move_speed();
	
	ATimeThiefCharacterBase* LocalPlayer = GetLocalPlayerPawn();
	if (LocalPlayer == nullptr)
	{
		return false;
	}
	
	if (auto* HealthComp = LocalPlayer->FindComponentByClass<UTimeThiefHealthComponent>())
	{
		HealthComp->SetHealth(MaxHealth, CurrentHealth);
	}
	
	if (auto* TimePointComp = LocalPlayer->FindComponentByClass<UTimePointSystemComponent>())
	{
		TimePointComp->SetTimePoints(TimePoints);
	}
	
	if (auto* CMC = LocalPlayer->GetCharacterMovement())
	{
		UE_LOG(LogTemp, Log, TEXT("[Network] Setting local player move speed Init: %f"), MoveSpeed);
		CMC->MaxWalkSpeed = MoveSpeed;
	}
	
	ATimeThiefMasterWeapon* WeaponActor = LocalPlayer->GetWeaponActor();
	if (WeaponActor == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] ApplyPlayerInitSetup: Missing weapon actor"));
		return false;
	}
	
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
		
		if (WeaponComp == nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Network] HandlePlayerInitSetup: Missing weapon component"));
			continue;
		}
		
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
	
	return true;
}

void UNetworkGameInstanceSubsystem::TryApplyPendingPlayerInitSetup()
{
	if (!bHasPendingPlayerInitSetup || bReceivedPlayerInitSetup)
	{
		return;
	}

	if (!ApplyPlayerInitSetup(PendingPlayerInitSetup))
	{
		if (UWorld* World = GetWorld())
		{
			if (!World->GetTimerManager().IsTimerActive(PlayerInitSetupRetryTimer))
			{
				World->GetTimerManager().SetTimer(
					PlayerInitSetupRetryTimer,
					this,
					&UNetworkGameInstanceSubsystem::TryApplyPendingPlayerInitSetup,
					EntitySpawnBatchIntervalSeconds,
					true);
			}
		}
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PlayerInitSetupRetryTimer);
	}

	bReceivedPlayerInitSetup = true;
	bHasPendingPlayerInitSetup = false;
	PendingPlayerInitSetup.Clear();
	TrySendLoadingComplete();
}

void UNetworkGameInstanceSubsystem::HandlePlayerGameResult(const se::game::N_PlayerGameResult& Pkt)
{
	check(IsInGameThread())
	
	if (!IsRoomPlayableState(PlayState))
	{
		return;
	}
	
	const uint32 Rank = Pkt.rank();
	const int32 Score = Pkt.score();
	FString PlayerName = UTF8_TO_TCHAR(Pkt.killer().c_str());
	if (PlayerName.IsEmpty())
	{
		PlayerName = TEXT("You are Victorious!");
	}
	
	UE_LOG(LogTemp, Log, TEXT("[Network] Game Result - Rank: %u, Score: %d, Killer: %s"), Rank, Score, *PlayerName);
	OnPlayerGameResult.Broadcast(static_cast<int32>(Rank), Score, PlayerName);
}

void UNetworkGameInstanceSubsystem::HandleGameDataInit(const se::game::N_GameDataInit& Pkt)
{
	check(IsInGameThread());

	StoreItemPrices.Reset();
	SoldOutStoreItems.Reset();

	for (const se::game::StoreEntryInitData& Entry : Pkt.store_entries())
	{
		StoreItemPrices.Add(Entry.store_item_id(), Entry.price());
		if (Entry.price() <= 0)
		{
			SoldOutStoreItems.Add(Entry.store_item_id());
		}
	}

	OnStorePriceDataUpdated.Broadcast();

	UE_LOG(LogTemp, Log, TEXT("[StorePkt] N_GameDataInit StoreEntries=%d"), Pkt.store_entries_size());
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
			if (!Pkt.has_monster_movement())
			{
				UE_LOG(LogTemp, Warning, TEXT("[Network] HandleMove: Missing monster_movement for monster entity"));
				return;
			}
			
			const auto& MonsterMovement = Pkt.monster_movement();
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

void UNetworkGameInstanceSubsystem::HandleDoubleJump(const se::game::N_DoubleJump& Pkt)
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

void UNetworkGameInstanceSubsystem::HandleWireLaunch(const se::game::N_WireLaunch& Pkt)
{
	check(IsInGameThread());

	if (!IsRoomPlayableState(PlayState))
	{
		return;
	}

	if (!Pkt.has_entity_id() || !Pkt.has_start_position() || !Pkt.has_direction())
	{
		UE_LOG(LogTemp, Warning, TEXT("[WirePkt][Stage=Apply][N_WireLaunch] missing required fields"));
		return;
	}

	const uint32 EntityId = Pkt.entity_id().value();
	const auto& Start = Pkt.start_position();
	const auto& Direction = Pkt.direction();
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
	check(IsInGameThread());
	
	if (!IsRoomPlayableState(PlayState))
	{
		return;
	}
	
	const uint32 EntityId = Pkt.entity_id().value();
	const uint32 AttackType = Pkt.attack_type();
	
	FRemoteAttackNotify Notify{};
	Notify.AttackerEntityId = EntityId;
	Notify.NotifyType = ECombatNotifyType::Attack;		// Attack이 아닐 수 있다
	// TODO: Attack Type 다른 거 올 수 도 있는거 확인하기 
	Notify.AttackId = AttackType;
	
	ApplyRemoteAttackNotifyToActor(EntityId, Notify);
}

void UNetworkGameInstanceSubsystem::HandleMonsterFire(const se::game::N_MonsterFire& Pkt)
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
	Notify.AttackId = Pkt.attack_type();
	const auto& Origin = Pkt.start_position();
	const auto& Dir = Pkt.direction();
	Notify.Origin = FVector(Origin.x(), Origin.y(), Origin.z());
	Notify.Direction = FVector(Dir.x(), Dir.y(), Dir.z());
	Notify.Range = Pkt.range();
	
	ApplyRemoteAttackNotifyToActor(EntityId, Notify);
}

void UNetworkGameInstanceSubsystem::HandleMonsterImpact(const se::game::N_MonsterImpact& Pkt)
{
	check(IsInGameThread());
	
	if (!IsRoomPlayableState(PlayState))
	{
		return;
	}
	
	const uint32 EntityId = Pkt.entity_id().value();
	FRemoteAttackNotify Notify{};
	Notify.AttackerEntityId = EntityId;
	Notify.NotifyType = ECombatNotifyType::Impact;
	Notify.AttackId = Pkt.attack_type();
	const auto& Position = Pkt.position();
	Notify.Origin = FVector(Position.x(), Position.y(), Position.z());
	
	ApplyRemoteAttackNotifyToActor(EntityId, Notify);
}

void UNetworkGameInstanceSubsystem::HandleMonsterTarget(const se::game::N_MonsterTarget& Pkt)
{
	check(IsInGameThread());
	
	if (!IsRoomPlayableState(PlayState))
	{
		return;
	}
	
	const uint32 EntityId = Pkt.monster_id().value();
	FEntityRuntimeEntry* EntityEntry = EntityEntries.Find(EntityId);
	if (EntityEntry == nullptr || EntityEntry->Actor == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] HandleMonsterTarget: Missing actor for EntityId=%u"), EntityId);
		return;
	}
	
	const uint32 TargetId = Pkt.target_id().value();
	FEntityRuntimeEntry* TargetEntry = EntityEntries.Find(TargetId);
	if (TargetEntry == nullptr and TargetId != 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] HandleMonsterTarget: Missing target actor for TargetId=%u"), TargetId);
	}
	
	if (auto MonsterPawn = Cast<ATimeThiefMonster>(EntityEntry->Actor.Get()))
	{
		MonsterPawn->SetTarget(TargetId, TargetEntry ? TargetEntry->Actor.Get() : nullptr);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] HandleMonsterTarget: Missing MonsterComponent for EntityId=%u"), EntityId);
	}
}

void UNetworkGameInstanceSubsystem::HandleThrowGrenade(const se::game::N_ThrowGrenade& Pkt)
{
	check(IsInGameThread());
	
	if (!IsRoomPlayableState(PlayState))
	{
		return;
	}
	
	const uint32 OwnerId = Pkt.owner_id().value();
	const uint32 EntityId = Pkt.entity_id().value();
	FRemoteAttackNotify Notify{};
	Notify.AttackerEntityId = OwnerId;
	Notify.SpawnEntityId = EntityId;
	Notify.NotifyType = ECombatNotifyType::Throw;
	Notify.WeaponId = Pkt.grenade_type();
	const auto& Origin = Pkt.start_position();
	Notify.Origin = FVector(Origin.x(), Origin.y(), Origin.z());
	const auto& Dir = Pkt.direction();
	Notify.Direction = FVector(Dir.x(), Dir.y(), Dir.z());
	
	ApplyRemoteAttackNotifyToActor(OwnerId, Notify);
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
	check(IsInGameThread());
	
	if (!IsRoomPlayableState(PlayState))
	{
		return;
	}
	
	ATimeThiefCharacterBase* LocalPlayer = GetLocalPlayerPawn();
	if (LocalPlayer == nullptr)
	{
		return;
	}
	
	ATimeThiefMasterWeapon* WeaponActor = LocalPlayer->GetWeaponActor();
	
	for (const auto& WeaponInfo : Pkt.stats())
	{
		const uint32 WeaponId = WeaponInfo.weapon_id();
		const auto& WeaponStat = WeaponInfo.stat();
		
		int MagCapacity = WeaponStat.mag_capacity();
		float FireInterval = WeaponStat.fire_interval();
		float ReloadTime = WeaponStat.reload_time();
		int32 PelletCount = WeaponStat.pellet_count();
		float ConeAngle = WeaponStat.cone_angle();
		float ProjectileSpeed = WeaponStat.projectile_speed();
		float ExplosionRadius = WeaponStat.explosion_radius();
		auto* WeaponComp = WeaponActor->GetWeaponComponentByTag(FTimeThiefGameplayTags::ResolveWeaponTagFromId(WeaponId));
		
		if (WeaponComp == nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Network] HandlePlayerInitSetup: Missing weapon component"));
			continue;
		}
		
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

void UNetworkGameInstanceSubsystem::HandleUseSkillRes(const se::game::S_UseSkillRes& Pkt)
{
	// Minsoo
	// 우선 존재하는 SkillBaseComponent를 사용하였음
	// 마음에 안들 경우 위 class를 갈아 엎거나 새로 만들어서 연결해도 무방
	const uint32 SlotIndex = Pkt.slot_index();							// 사용하려 한 스킬 슬롯 인덱스
	const uint32 SkillId = Pkt.skill_id();								// 사용하려 한 스킬 ID
	const uint64 CooldownEndMs = Pkt.cooldown_end_ms();					// 스킬 쿨다운이 끝나는 절대 시간 (ms) <- 서버 시간 기준 (신뢰하여 사용하면 곤란함)
	const uint32 RemainingCooldownMs = Pkt.remaining_cooldown_ms();		// 스킬이 다시 사용 가능해질 때까지 남은 시간 (ms)

	if (!Pkt.success())
	{
		// 사용 실패 시 로그
		const auto& Result = Pkt.result();
		// 쿨타임도 RemainingCooldownMs로 설정해줘야 함 (아래 코드)
		ApplyUseSkillCooldown(Pkt);
		UE_LOG(LogTemp, Warning, TEXT("[SkillPkt] Use skill failed. Slot=%u, SkillId=%u, RemainingCooldownMs=%u, Result=%s"),
			SlotIndex,
			SkillId,
			RemainingCooldownMs,
			UTF8_TO_TCHAR(Result.message().c_str()));
		return;
	}

	// 사용 성공 시 스킬 쿨다운 적용 및 로그
	ApplyUseSkillCooldown(Pkt);
	UE_LOG(LogTemp, Log, TEXT("[SkillPkt] Use skill accepted. Slot=%u, SkillId=%u, CooldownEndMs=%llu, RemainingCooldownMs=%u"),
		SlotIndex,
		SkillId,
		static_cast<unsigned long long>(CooldownEndMs),
		RemainingCooldownMs);
}

void UNetworkGameInstanceSubsystem::HandleUseSkill(const se::game::N_UseSkill& Pkt)
{
	// Minsoo
	// 스킬 사용에 대한 다양한 효과(발사체, 버프, 이동 등)를 처리하기 위한 패킷
	
	check(IsInGameThread());

	if (!IsRoomPlayableState(PlayState))
	{
		return;
	}

	const uint32 EntityId = Pkt.has_entity_id() ? Pkt.entity_id().value() : 0;		// 스킬을 사용한 Entity의 ID
	const uint32 SkillId = Pkt.skill_id();				// 사용한 스킬의 ID
	const uint32 SlotIndex = Pkt.slot_index();			// 사용한 스킬 슬롯 인덱스
	const uint64 StartedAtMs = Pkt.started_at_ms();		// 스킬 사용이 시작된 절대 시간 (ms) <- 서버 시간 기준 (디버깅용, 실제 사용 X)
	const uint32 DurationMs = Pkt.duration_ms();		// 스킬 효과의 지속 시간 (ms) <- (역행의 경우 돌아가는데 걸리는 시간, 1000ms 라면 1초 안에 원래 상태로 돌아가도록)
	// 이 EntityId가 LocalPlayer의 경우 직접 효과 적용
	const bool bIsLocalPlayer = IsLocalPlayerEntity(EntityId);
	FEntityRuntimeEntry* EntityEntry = EntityEntries.Find(EntityId);
	AActor* SourceActor = EntityEntry != nullptr ? EntityEntry->Actor.Get() : nullptr;

	switch (Pkt.detail_case())
	{
	case se::game::N_UseSkill::kTimeAccel:		// 이때의 SKill Id 는 1 (KTimeAccel이랑은 다른 것임)
		{
			const auto& TimeAccel = Pkt.time_accel();
			const uint32 FireRateBonusPercent = TimeAccel.fire_rate_bonus_percent();		// 전투 관련 가속 효과 (n% <- n은 정수) 
			const uint32 MoveSpeedBonusPercent = TimeAccel.move_speed_bonus_percent();		// 이동 관련 가속 효과 (n% <- n은 정수)

			// TODO: 우선 해당 스킬을 사용한 Entity Id에게 시각 효과가 나타나야 할 거 같음
			//	     그리고 LocalPlayer의 경우 실제 버프가 들어가고 Duration이 지나면 버프 효과가 사라져야 함
			//		 일단은 FireRate는 전투 관련으로 발사 속도, 재장전 속도가 % 만큼 빨라지는 효과로, MoveSpeed는 이동 관련으로 걷기/뛰기 속도가 % 만큼 빨라지는 효과로 가정하고 로그에 출력함
			UE_LOG(LogTemp, Log, TEXT("[SkillPkt] N_UseSkill TimeAccel. Entity=%u, SkillId=%u, Slot=%u, DurationMs=%u, FireRateBonus=%u, MoveSpeedBonus=%u"),
				EntityId,
				SkillId,
				SlotIndex,
				DurationMs,
				FireRateBonusPercent,
				MoveSpeedBonusPercent);
			break;
		}
	case se::game::N_UseSkill::kAfterImage:		// 이때의 SKill Id 는 2 (KAfterImage이랑은 다른 것임)
		{
			const auto& AfterImage = Pkt.after_image();
			const FVector StartPosition = ToVector(AfterImage.start_position());	// 유령(더미)이 Spawn 되는 위치
			const FVector Direction = ToVector(AfterImage.direction());				// 유령(더미)이 이동하는 방향 (단위 벡터)
			const float MoveSpeed = AfterImage.move_speed();						// 유령(더미)의 이동 속도 (단위: Unreal units per second)

			// TODO: 모든 Client에서 StartPos에서 Spawn 시킨 유령(더미)가 Direction으로 이동하게
			//	     Duration 동안 지속되고 Duration이 끝나면 자연 소멸(Despawn) 되도록
			UE_LOG(LogTemp, Log, TEXT("[SkillPkt] N_UseSkill AfterImage. Entity=%u, SkillId=%u, Slot=%u, DurationMs=%u, Start=(%.1f, %.1f, %.1f), Dir=(%.2f, %.2f, %.2f), MoveSpeed=%.2f"),
				EntityId,
				SkillId,
				SlotIndex,
				DurationMs,
				StartPosition.X,
				StartPosition.Y,
				StartPosition.Z,
				Direction.X,
				Direction.Y,
				Direction.Z,
				MoveSpeed);
			break;
		}
	case se::game::N_UseSkill::kRewind:		// 이때의 SKill Id 는 3 (KRewind이랑은 다른 것임)
		{
			const auto& Rewind = Pkt.rewind();
			const uint32 RewindDurationMs = Rewind.rewind_duration_ms();				// 역행으로 돌아가는 DeltaTime 지점 (ex. 3000ms 라면 3초 전으로 돌아가는 것) 
			const uint32 InvulnerableDurationMs = Rewind.invulnerable_duration_ms();	// 역행을 시전하는 동안 무적이 되는 시간 (Duration 값이랑 같음)
			const int32 TargetHealth = Rewind.target_health();							// 역행이 끝났을 때 돌아갈 체력 (HP)
			const FVector TargetPosition = Rewind.has_target_position() ? ToVector(Rewind.target_position()) : FVector::ZeroVector;	// 역행이 끝났을 때 돌아갈 위치 (만약 패킷에 포함되어 있지 않다면 Local에서 알아서 처리)

			// TODO: 체력의 경우 바로 세팅해도 되고, 위치의 경우는 Client에서 직접 Local Player의 현재 위치를 일정 주기마자 저장해 두고
			//	     현재 패킷이 도착한 경우 RewindDurationMs의 시각으로 천천히 돌아가도록 (테이프 뒤로 감듯이)
			UE_LOG(LogTemp, Log, TEXT("[SkillPkt] N_UseSkill Rewind. Entity=%u, SkillId=%u, Slot=%u, DurationMs=%u, RewindMs=%u, InvulnerableMs=%u, TargetHealth=%d, Target=(%.1f, %.1f, %.1f)"),
				EntityId,
				SkillId,
				SlotIndex,
				DurationMs,
				RewindDurationMs,
				InvulnerableDurationMs,
				TargetHealth,
				TargetPosition.X,
				TargetPosition.Y,
				TargetPosition.Z);
			break;
		}
	case se::game::N_UseSkill::DETAIL_NOT_SET:
	default:
		UE_LOG(LogTemp, Warning, TEXT("[SkillPkt] N_UseSkill without detail. Entity=%u, SkillId=%u, Slot=%u, StartedAtMs=%llu, DurationMs=%u"),
			EntityId,
			SkillId,
			SlotIndex,
			static_cast<unsigned long long>(StartedAtMs),
			DurationMs);
		break;
	}

	(void)bIsLocalPlayer;
	(void)SourceActor;
}

void UNetworkGameInstanceSubsystem::HandleKillPlayer(const se::game::N_KillPlayer& Pkt)
{
}

void UNetworkGameInstanceSubsystem::HandleReloadRes(const se::game::S_ReloadRes& Pkt)
{
	check(IsInGameThread());
	
	if (!IsRoomPlayableState(PlayState))
	{
		return;
	}
	
	const bool bSuccess = Pkt.success();
	if (!bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to reload (From Server)"));
		return;
	}
	
	const uint32 WeaponId = Pkt.weapon_id();
	const uint32 ReloadedAmmo = Pkt.reloaded_ammo();	// Delta Ammo
	const uint32 RemainingAmmo = Pkt.remaining_ammo();	// 현재 장전된 탄약
	
	if (auto LocalPlayerPawn = GetLocalPlayerPawn())
	{
		if (auto CombatComp = LocalPlayerPawn->FindComponentByClass<UTimeThiefPlayerCombatComponent>())
		{
			if (auto MasterWeapon = CombatComp->GetMasterWeapon())
			{
				const FGameplayTag WeaponTag = FTimeThiefGameplayTags::ResolveWeaponTagFromId(WeaponId);
				if (auto WeaponComp = MasterWeapon->GetWeaponComponentByTag(WeaponTag))
				{
					WeaponComp->HandleReloadResult(ReloadedAmmo, RemainingAmmo);
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("Received ReloadRes for unknown WeaponId=%u"), WeaponId);
				}
			}
		}
	}
}

void UNetworkGameInstanceSubsystem::HandleEntityHit(const se::game::N_EntityHit& Pkt)
{
	check(IsInGameThread());
	
	if (!IsRoomPlayableState(PlayState))
	{
		return;
	}
	
	// UE_LOG(LogTemp, Log, TEXT("[Network] HandleEntityHit: EntityId=%u"), Pkt.entity_id().value());
	const uint32 TargetEntityId = Pkt.entity_id().value();
	const FVector HitPosition = FVector(Pkt.hit_position().x(), Pkt.hit_position().y(), Pkt.hit_position().z());
	
	if (TargetEntityId == 0) 
	{
		// TODO: Entity가 아니라 벽 같은 것에 맞은 것
	}
	else
	{
		FRemoteAttackNotify Notify{};
		Notify.NotifyType = ECombatNotifyType::Hit;
		Notify.Origin = HitPosition;
	
		ApplyRemoteAttackNotifyToActor(TargetEntityId, Notify);
	}
}

void UNetworkGameInstanceSubsystem::HandleGrenadeMoveSync(const se::game::N_GrenadeMoveSync& Pkt)
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
		UE_LOG(LogTemp, Warning, TEXT("Failed to find actor for grenade move sync. EntityId=%u"), EntityId);
		return;
	}
	
	if (auto* GrenadeComp = Cast<ATimeThiefThrowableProjectile>(EntityEntry->Actor.Get()))
	{
		const auto& Position = Pkt.position();
		const auto& Rotation = Pkt.rotation();
		const auto& Velocity = Pkt.velocity();
		FThrowableMoveSnapshot Snapshot;
		Snapshot.ObjectId = EntityId;
		Snapshot.Location = FVector(Position.x(), Position.y(), Position.z());
		Snapshot.Rotation = FRotator(Rotation.pitch(), Rotation.yaw(), Rotation.roll());
		Snapshot.Velocity = FVector(Velocity.x(), Velocity.y(), Velocity.z());
		GrenadeComp->PushRemoteMoveSnapshot(Snapshot);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to find grenade component for move sync. EntityId=%u Actor=%s"), EntityId, *GetNameSafe(EntityEntry->Actor.Get()));
	}
}

void UNetworkGameInstanceSubsystem::HandleGrenadeExplosion(const se::game::N_GrenadeExplosion& Pkt)
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
		UE_LOG(LogTemp, Warning, TEXT("Failed to find actor for grenade explosion. EntityId=%u"), EntityId);
		return;
	}
	
	if (auto* GrenadeComp = Cast<ATimeThiefThrowableProjectile>(EntityEntry->Actor.Get()))
	{
		const FVector ExplosionLocation = FVector(Pkt.position().x(), Pkt.position().y(), Pkt.position().z());
		GrenadeComp->RemoteExplosionEffect(ExplosionLocation);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to find grenade component for explosion. EntityId=%u Actor=%s"), EntityId, *GetNameSafe(EntityEntry->Actor.Get()));
	}
	
	RemoveEntity(EntityId);
}

void UNetworkGameInstanceSubsystem::HandleProjectileExplosion(const se::game::N_ProjectileExplosion& Pkt)
{
	check(IsInGameThread());
	
	if (!IsRoomPlayableState(PlayState))
	{
		return;
	}
	
	// UE_LOG(LogTemp, Log, TEXT("[Network] HandleProjectileExplosion: EntityId=%u"), Pkt.entity_id().value());
	
	const uint32 EntityId = Pkt.entity_id().value();
	FEntityRuntimeEntry* EntityEntry = EntityEntries.Find(EntityId);
	if (EntityEntry == nullptr || EntityEntry->Actor == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to find actor for projectile explosion. EntityId=%u"), EntityId);
		return;
	}
	
	if (auto* ProjectileComp = Cast<ATimeThiefRocketProjectile>(EntityEntry->Actor.Get()))
	{
		// UE_LOG(LogTemp, Log, TEXT("[Network] ExplodeSyncNetwork: EntityId=%u"), Pkt.entity_id().value());
		
		const FVector ExplosionLocation = FVector(Pkt.position().x(), Pkt.position().y(), Pkt.position().z());
		ProjectileComp->ExplodeSyncNetwork(ExplosionLocation);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to find projectile component for explosion. EntityId=%u Actor=%s"), EntityId, *GetNameSafe(EntityEntry->Actor.Get()));
	}
	
	// TODO: 이제 해당 Entity 제거하는 로직
}

void UNetworkGameInstanceSubsystem::HandleWeaponStatChanged(const se::game::N_WeaponStatChanged& Pkt)
{
	check(IsInGameThread());
	
	if (!IsRoomPlayableState(PlayState))
	{
		return;
	}
	
	ATimeThiefCharacterBase* LocalPlayer = GetLocalPlayerPawn();
	if (LocalPlayer == nullptr)
	{
		return;
	}
	
	const uint32 WeaponId = Pkt.weapon_id();
	ATimeThiefMasterWeapon* WeaponActor = LocalPlayer->GetWeaponActor();
	if (WeaponActor == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] HandleWeaponStatChanged: Missing master weaponActor"));
		return;
	}
	
	auto* WeaponComp = WeaponActor->GetWeaponComponentByTag(FTimeThiefGameplayTags::ResolveWeaponTagFromId(WeaponId));
		
	if (WeaponComp == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] HandleWeaponStatChanged: Missing weapon component"));
		return;
	}
	
	// 현재 WeaponStat 받아와서
	FWeaponStatData StatData = WeaponComp->GetWeaponStatDataForNetwork();
	
	// 바뀐 부분의 Stat 기록하고
	for (const auto& WeaponStatValue : Pkt.stats())
	{
		switch (WeaponStatValue.stat_type())
		{
		case se::game::WEAPON_STAT_MAGAZINE_SIZE:
			StatData.MagCapacity = WeaponStatValue.int_value();
			break;
		case se::game::WEAPON_STAT_FIRE_INTERVAL:
			StatData.FireInterval = WeaponStatValue.float_value();
			break;
		case se::game::WEAPON_STAT_RELOAD:
			StatData.ReloadTime = WeaponStatValue.float_value();
			break;
			
		case se::game::WEAPON_STAT_PALLET:
			StatData.PelletCount = WeaponStatValue.int_value();
			break;
		case se::game::WEAPON_STAT_CONE:
			StatData.ConeAngle = WeaponStatValue.float_value();
			break;
			
		case se::game::WEAPON_STAT_PROJECTILE_SPEED:
			StatData.ProjectileSpeed = WeaponStatValue.float_value();
			break;
		case se::game::WEAPON_STAT_EXPLOSION_RADIUS:
			StatData.ExplosionRadius = WeaponStatValue.float_value();
			break;
		}
	}
	
	// 실제 Weapon에 적용 (업데이트)
	WeaponComp->SetWeaponStatForNetwork(StatData);
}

void UNetworkGameInstanceSubsystem::HandleUseItem(const se::game::N_UseItem& Pkt)
{
	// TODO: 효과를 적용 하는 게 아닌 이펙트 연출을 위해서 (ex. 붕대 감는 모션 등)
}

void UNetworkGameInstanceSubsystem::HandleSetSavePointRes(const se::game::S_SetSavePointRes& Pkt)
{
	if (!Pkt.success())
	{
		const auto& Result = Pkt.result();
		
		UE_LOG(LogTemp, Warning, TEXT("Failed to set save point: %s"), UTF8_TO_TCHAR(Result.message().c_str()));
		return;
	}
	
	const auto& Pos = Pkt.position();
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
	check(IsInGameThread());
	
	UE_LOG(LogTemp, Log, TEXT("PickupItem received"));
	const uint32 PlayerID = Pkt.entity_id().value();
	const uint32 ItemID = Pkt.item_entity_id().value();
	
	UE_LOG(LogTemp, Log, TEXT("Entity %u picked up item %u"), PlayerID, ItemID);
	
	auto ItemEntry = EntityEntries.Find(ItemID);
	if (ItemEntry == nullptr || !ItemEntry->Actor.IsValid())
	{
		return;
	}
	if (auto ItemActor = Cast<AItemBase>(ItemEntry->Actor.Get()))
	{
		ItemActor->Disable();
			
		EntityEntries.Remove(ItemID);
	}

	// TODO: 해당 Player가 PickUp 하는 모션 1회 Play
	// EntityEntry->Actor->PlayPickUp()
	// A 플레이어가 아이템 먹음
}

void UNetworkGameInstanceSubsystem::HandleUseStoreRes(const se::game::S_UseStoreRes& Pkt)
{
	if (!Pkt.success())
	{
		return;
	}

	const int32 NewPrice = Pkt.new_price();
	if (NewPrice > 0)
	{
		StoreItemPrices.Add(Pkt.store_item_id(), NewPrice);
	}

	if (Pkt.is_sold_out())
	{
		SoldOutStoreItems.Add(Pkt.store_item_id());
	}
	else
	{
		SoldOutStoreItems.Remove(Pkt.store_item_id());
	}
	OnStorePriceDataUpdated.Broadcast();
	OnStorePurchaseSucceeded.Broadcast(Pkt.store_item_id(), NewPrice, Pkt.is_sold_out());
}

void UNetworkGameInstanceSubsystem::HandleItemGained(const se::game::N_ItemGained& Pkt)
{
	check(IsInGameThread());
	
	ATimeThiefCharacterBase* LocalPlayer = GetLocalPlayerPawn();
	if (LocalPlayer == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to get local player pawn"));
		return;
	}
	auto* Player = Cast<ATimeThiefPlayerCharacter>(LocalPlayer);
	if (Player == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to get player pawn"));
		return;
	}
	
	UInventorySystemComponent* InventoryComp = Player->GetInventoryComponent();
	if (InventoryComp == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to get inventory component"));
		return;
	}
	
	uint32 ItemId = Pkt.item_id();
	uint32 NewCount = Pkt.new_quantity();
	uint32 Delta = Pkt.quantity();
	
	// TODO: 일단 임시로 이렇게, 최종적으론 NewCount로 Set 할 수 있게끔	
	InventoryComp->AddItem(static_cast<EItemID>(ItemId), Delta);

	// 형일이 주석
	// 실제로 아이템이 늘어나는 곳
	// 저 패킷에 뭐뭐 있는지
	// 
}

void UNetworkGameInstanceSubsystem::HandleChestInteracted(const se::game::N_ChestInteracted& Pkt)
{
	check(IsInGameThread());
	
	const uint32 ChestEntityId = Pkt.chest_entity_id().value();
	FEntityRuntimeEntry* ChestEntry = EntityEntries.Find(ChestEntityId);
	
	if (ChestEntry == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[Chest] Chest entity entry not found. ChestEntityId=%u"), ChestEntityId);
		return;
	}

	AChestActor* ChestActor = Cast<AChestActor>(ChestEntry->Actor.Get());
	if (ChestActor == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[Chest] Chest actor not found or invalid type. ChestEntityId=%u"), ChestEntityId);
		return;
	}

	ChestActor->OpenChest();

	// TODO: 해당 Player가 Chest에 Interaction 하는 모션 1회 Play
	// const uint32 PlayerEntityId = Pkt.entity_id().value();
}

void UNetworkGameInstanceSubsystem::HandleItemLost(const se::game::N_ItemLost& Pkt)
{
	check(IsInGameThread());
	
	ATimeThiefCharacterBase* LocalPlayer = GetLocalPlayerPawn();
	if (LocalPlayer == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to get local player pawn"));
		return;
	}
	auto* Player = Cast<ATimeThiefPlayerCharacter>(LocalPlayer);
	if (Player == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to get player pawn"));
		return;
	}
	
	UInventorySystemComponent* InventoryComp = Player->GetInventoryComponent();
	if (InventoryComp == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to get inventory component"));
		return;
	}
	
	uint32 ItemId = Pkt.item_id();
	uint32 NewCount = Pkt.new_quantity();
	uint32 LostCount = Pkt.quantity();
	
	// TODO: 일단 임시로 이렇게, 최종적으론 NewCount로 Set 할 수 있게끔	
	InventoryComp->RemoveItem(static_cast<EItemID>(ItemId), LostCount);
}

void UNetworkGameInstanceSubsystem::HandleItemSnapshot(const se::game::N_ItemSnapshot& Pkt)
{
	check(IsInGameThread());
	
	ATimeThiefCharacterBase* LocalPlayer = GetLocalPlayerPawn();
	if (LocalPlayer == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to get local player pawn"));
		return;
	}
	auto* Player = Cast<ATimeThiefPlayerCharacter>(LocalPlayer);
	if (Player == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to get player pawn"));
		return;
	}
	
	UInventorySystemComponent* InventoryComp = Player->GetInventoryComponent();
	if (InventoryComp == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to get inventory component"));
		return;
	}
	
	InventoryComp->ClearInventory();
	
	for (const auto& Item : Pkt.items())
	{
		 uint32 ItemId = Item.item_id();
		 uint32 Quantity = Item.amount();
		 
		 InventoryComp->AddItem(static_cast<EItemID>(ItemId), Quantity);
	}
}

void UNetworkGameInstanceSubsystem::HandleEquipItem(const se::game::N_EquipItem& Pkt)
{
	// TODO: 해당 플레이어 (Local이 아닌)가 특정 아이템을 손에 들고 있는 모션을 위해 (붕대를 손에 든다, 수류탄을 손에 든다)
}

void UNetworkGameInstanceSubsystem::HandleEquipItemRes(const se::game::S_EquipItemRes& Pkt)
{
	// TODO: 만약 유효하지 않은 아이템 장착 요청이었을 경우 (Local Player의 경우)
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
	
	if (auto TTCharacter = Cast<ATimeThiefCharacterBase>(Actor))
	{		// 캐릭터가 죽는 경우에만 OnDeath 호출, 몬스터나 다른 Entity는 별도의 연출이 필요할 수 있음
		// if (EntityId == LocalPlayerEntityId)
		// {
		// 	// 로컬 플레이어 사망 처리 후 컨트롤 불가하게
		// 	TTCharacter->OnDeath();
		// }
		// else
		// {
		// 	// 타 플레이어는 사망 연출 진행
		// 	TTCharacter->OnDeath();
		// }
		
		TTCharacter->OnDeath();
	}
	else if (auto Monster = Cast<ATimeThiefMonster>(Actor))
	{
		// 몬스터 사망 처리
		Monster->OnDeathNetwork();
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
	
	if (auto TTCharacter = Cast<ATimeThiefCharacterBase>(Actor))
	{		// 캐릭터가 부활하는 경우에만 OnRespawn 호출, 몬스터나 다른 Entity는 별도의 연출이 필요할 수 있음
		// if (EntityId == LocalPlayerEntityId)
		// {
		// 	// 로컬 플레이어 부활 처리 후 컨트롤 가능하게
		// 	TTCharacter->HandleRespawnFromServer(RespawnPosition);
		// }
		// else
		// {
		// 	// 타 플레이어는 부활 연출 진행
		// 	TTCharacter->HandleRespawnFromServer(RespawnPosition);
		// }
		
		TTCharacter->HandleRespawnFromServer(RespawnPosition);
	}
	else if (auto Monster = Cast<ATimeThiefMonster>(Actor))
	{
		FRotator Rotation(0.f, Yaw, 0.f);
		
		// 몬스터 부활 처리
		Monster->OnRespawnNetwork(RespawnPosition, Rotation);
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
		UE_LOG(LogTemp, Warning, TEXT("[Network] Local player has no UTime)PointSystemComponent"));
		return;
	}
	
	TimePointComp->SetTimePoints(Pkt.time_points());
}

void UNetworkGameInstanceSubsystem::HandleSkillUnlock(const se::game::N_SkillUnlock& Pkt)
{
	// 해당 스킬은 이제 사용 가능한 스킬이 되었음 (UI에서 잠금 해제된 것으로 업데이트)
	UE_LOG(LogTemp, Log, TEXT("Skill unlocked: SkillId=%u"), Pkt.skill_id());
}

void UNetworkGameInstanceSubsystem::HandleSkillEquipRes(const se::game::S_SkillEquipRes& Pkt)
{
	// 사용하지 않음
	// UI가 있다면 사용 가능해짐
	// Client 상에서 UI로 장착 요청을 하고 그에 따른 피드백 용도임
	// 이 패킷이 오면 UI에서 해당 슬롯이 장착된 것으로 업데이트
	
	UE_LOG(LogTemp, Log, TEXT("Skill equip result: SkillId=%u, SlotIndex=%u, Success=%s"), Pkt.skill_id(), Pkt.slot_index(), Pkt.success() ? TEXT("true") : TEXT("false"));
}

void UNetworkGameInstanceSubsystem::HandleSkillUnlockSnapshot(const se::game::N_SkillUnlockSnapshot& Pkt)
{
	// Minsoo
	
	check(IsInGameThread());
	
	if (!IsRoomPlayableState(PlayState))
	{
		return;
	}
	
	ATimeThiefCharacterBase* LocalPlayer = GetLocalPlayerPawn();
	// Skill System에 통지하도록
	// Pkt의 unlocked_skill_ids에 들어있는 값들은 사용 가능한 스킬들
	// equipped_skill_slots에 있는 값은 slot_index와 skill_id로 이루어져 있고, 현재 장착된 스킬 슬롯 정보임
}

void UNetworkGameInstanceSubsystem::HandleSkillEquip(const se::game::N_SkillEquip& Pkt)
{
	// TODO: 서버에서 Skill 구매 시 빈 슬롯으로 자동 장착된 경우 UI에서 해당 슬롯이 장착된 것으로 업데이트
	UE_LOG(LogTemp, Log, TEXT("Skill equipped: SkillId=%u, SlotIndex=%u"), Pkt.skill_id(), Pkt.slot_index());
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
	check(IsInGameThread());
	
	if (!IsRoomPlayableState(PlayState))
	{
		return;
	}
	
	auto* Player = GetLocalPlayerPawn();
	auto HealthComp = Player ? Player->FindComponentByClass<UTimeThiefHealthComponent>() : nullptr;
	if (HealthComp == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] Local player has no UTimeThiefHealthComponent"));
		return;
	}
	
	HealthComp->SetHealth(Pkt.max_health(), Pkt.current_health());
}

void UNetworkGameInstanceSubsystem::HandleSpeedChanged(const se::game::N_SpeedChanged& Pkt)
{
	check(IsInGameThread());
	
	ATimeThiefCharacterBase* LocalPlayer = GetLocalPlayerPawn();
	if (LocalPlayer == nullptr)
	{
		return;
	}
	
	if (auto* CMC = LocalPlayer->GetCharacterMovement())
	{
		UE_LOG(LogTemp, Log, TEXT("[Network] HandleSpeedChanged local player move speed: %f"), Pkt.new_speed());
		CMC->MaxWalkSpeed = Pkt.new_speed();
	}
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

void UNetworkGameInstanceSubsystem::HandleDebugDraw(const se::game::N_DebugDraw& Pkt)
{
	check(IsInGameThread());
	
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const FColor Color = ToDebugDrawColor(Pkt.color_rgba());
	const float Duration = ToDebugDrawDuration(Pkt.duration());
	const float Thickness = ToDebugDrawThickness(Pkt.thickness());
	
	switch (Pkt.shape_case())
	{
	case se::game::N_DebugDraw::kSphere:
	{
		const auto& Sphere = Pkt.sphere();
		DrawDebugSphere(
			World,
			ToVector(Sphere.position()),
			Sphere.radius(),
			DebugDrawSphereSegments,
			Color,
			false,
			Duration,
			0,
			Thickness);
		break;
	}
	case se::game::N_DebugDraw::kObb:
	{
		const auto& Obb = Pkt.obb();
		DrawDebugBox(
			World,
			ToVector(Obb.center()),
			ToVector(Obb.half_extents()),
			ToRotator(Obb.rotation()).Quaternion(),
			Color,
			false,
			Duration,
			0,
			Thickness);
		break;
	}
	default:
		UE_LOG(LogTemp, Warning, TEXT("HandleDebugDraw: shape is not set"));
		break;
	}
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

void UNetworkGameInstanceSubsystem::RemoveEntitiesByObjectType(se::common::ObjectType ObjectType)
{
	TArray<uint32> EntityIds;
	for (const TPair<uint32, FEntityRuntimeEntry>& Pair : EntityEntries)
	{
		if (Pair.Value.State.ObjectType == ObjectType)
		{
			EntityIds.Add(Pair.Key);
		}
	}

	for (const uint32 EntityId : EntityIds)
	{
		RemoveEntity(EntityId);
	}
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
		if (SpawnData->RocketProjectileClass)
		{
			return SpawnData->RocketProjectileClass;
		}
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
	else if (EntityState.ObjectType == se::common::OBJ_STORE)
	{
		if (SpawnData->StoreClass)
		{
			return SpawnData->StoreClass;
		}
	}
	else if (EntityState.ObjectType == se::common::OBJ_MONSTER)
	{
		switch (EntityState.TemplateId)
		{
		case 2:
			if (SpawnData->CatMonster)
			{
				return SpawnData->CatMonster;
			}
			
		case 3:
			if (SpawnData->MinionMonster)
			{
				return SpawnData->MinionMonster;
			}
			
		case 4:
			if (SpawnData->BossMonster)
			{
				return SpawnData->BossMonster;
			}
			
		default:
			if (SpawnData->TestMonster)
			{
				return SpawnData->TestMonster;
			}
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

// Minsoo
// 실제 Skill 구현과 연결은 어떻게 될 지 몰라서 다음과 같이 작성하여 남겨두었음,
// 본인 구현에 맞게 수정하는 것이 좋아보임

USkillBaseComponent* UNetworkGameInstanceSubsystem::FindLocalSkillComponent(uint32 SkillId)
{
	if (SkillId == 0)
	{
		return nullptr;
	}

	ATimeThiefPlayerCharacter* LocalPlayerPawn = GetLocalPlayerPawn();
	if (LocalPlayerPawn == nullptr)
	{
		return nullptr;
	}

	TArray<USkillBaseComponent*> SkillComponents;
	LocalPlayerPawn->GetComponents<USkillBaseComponent>(SkillComponents);
	for (USkillBaseComponent* SkillComponent : SkillComponents)
	{
		if (SkillComponent != nullptr && SkillComponent->GetSkillId() == SkillId)
		{
			return SkillComponent;
		}
	}

	return nullptr;
}

void UNetworkGameInstanceSubsystem::ApplyUseSkillCooldown(const se::game::S_UseSkillRes& Pkt)
{
	const uint32 RemainingCooldownMs = Pkt.remaining_cooldown_ms();
	USkillBaseComponent* SkillComponent = FindLocalSkillComponent(Pkt.skill_id());
	if (SkillComponent == nullptr)
	{
		if (RemainingCooldownMs > 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("[SkillPkt] Could not apply skill cooldown. SkillId=%u, RemainingCooldownMs=%u"),
				Pkt.skill_id(),
				RemainingCooldownMs);
		}
		return;
	}

	SkillComponent->ApplyServerCooldownMs(RemainingCooldownMs);
}

void UNetworkGameInstanceSubsystem::ResetLoadingGate()
{
	CancelPendingEntitySpawn();
	PendingPlayerInitSetup.Clear();
	bHasPendingPlayerInitSetup = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PlayerInitSetupRetryTimer);
	}

	bReceivedRoomEnterRes = false;
	bReceivedEntitiesSpawn = false;
	bReceivedPlayerInitSetup = false;
	bSentLoadingComplete = false;
}

void UNetworkGameInstanceSubsystem::SetLocalPlayerInputEnabled(bool bEnabled)
{
	UWorld* World = GetWorld();
	if (!World)
		return;

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
		return;

	if (bEnabled)
	{
		PC->SetIgnoreMoveInput(false);
		PC->SetIgnoreLookInput(false);

		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = false;
	}
	else
	{
		PC->SetIgnoreMoveInput(true);
		PC->SetIgnoreLookInput(true);

		// 로딩 UI가 있으면 UIOnly 또는 GameAndUI 사용 가능
		FInputModeUIOnly InputMode;
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = true;
	}
}

void UNetworkGameInstanceSubsystem::ResetTimeStormState()
{
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

	TimeStormComp->ReStart();
}

void UNetworkGameInstanceSubsystem::TrySendLoadingComplete()
{
	if (bSentLoadingComplete)
		return;

	if (!bReceivedRoomEnterRes)
		return;

	if (!bReceivedEntitiesSpawn)
		return;

	if (!bReceivedPlayerInitSetup)
		return;

	bSentLoadingComplete = true;
	SetPlayState(ENetworkPlayState::WaitingGameStart);

	se::game::C_LoadingCompleteReq Req;
	auto Buffer = ClientPacketHandler::MakeSendBuffer(Req);
	SendPacket(Buffer);

	UE_LOG(LogTemp, Log, TEXT("[Network] Sent C_LoadingCompleteReq"));
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

	SetPlayState(ENetworkPlayState::MatchMaking);

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

void UNetworkGameInstanceSubsystem::RequestRoomLeave()
{
	if (bIsConnected == false || GameSession == nullptr)
	{
		return;
	}

	if (PlayState == ENetworkPlayState::LeavingRoom)
	{
		UE_LOG(LogTemp, Log, TEXT("[Network] Ignore room leave request: leave already pending"));
		return;
	}

	if (PlayState != ENetworkPlayState::InRoom)
	{
		UE_LOG(LogTemp, Log, TEXT("[Network] Ignore room leave request: invalid state %d"), static_cast<int32>(PlayState));
		return;
	}

	se::room::C_RoomLeaveReq Request;
	auto SendBuffer = ClientPacketHandler::MakeSendBuffer(Request);
	SendPacket(SendBuffer);
	SetPlayState(ENetworkPlayState::LeavingRoom);
	UE_LOG(LogTemp, Log, TEXT("[Network] Sent C_RoomLeaveReq to server"));
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
	ResetTimeStormState();
	StoreItemPrices.Reset();
	SoldOutStoreItems.Reset();

	if (IsRoomStateCleared())
	{
		UE_LOG(LogTemp, Log, TEXT("[Network] ClearRoomState skipped: already cleared"));
		return;
	}

	TArray<TWeakObjectPtr<AActor>> ActorsToDestroy;
	ActorsToDestroy.Reserve(EntityEntries.Num());

	for (const auto& Pair : EntityEntries)
	{
		if (Pair.Value.Actor.IsValid())
		{
			ActorsToDestroy.Add(Pair.Value.Actor);
		}
	}

	EntityEntries.Empty();
	LocalPlayerEntityId = 0;
	RoomState = FRoomState();
	ResetLoadingGate();
	bRoomStateCleared = true;

	for (const TWeakObjectPtr<AActor>& ActorPtr : ActorsToDestroy)
	{
		if (ActorPtr.IsValid())
		{
			ActorPtr->Destroy();
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("[Network] Room state cleared. DestroyedActors=%d"), ActorsToDestroy.Num());
}

bool UNetworkGameInstanceSubsystem::IsRoomStateCleared() const
{
	return bRoomStateCleared && EntityEntries.Num() == 0 && LocalPlayerEntityId == 0 && RoomState.RoomId == 0;
}

void UNetworkGameInstanceSubsystem::NetworkEntryAdd(uint32 EntityId, const FEntityRuntimeEntry& Entry)
{
	EntityEntries.Add(EntityId, Entry);
}

void UNetworkGameInstanceSubsystem::NetworkEntryRemove(uint32 EntityId)
{
	EntityEntries.Remove(EntityId);
}

void UNetworkGameInstanceSubsystem::GetStoreActors(TArray<AStoreActor*>& OutStoreActors) const
{
	OutStoreActors.Reset();

	for (const TPair<uint32, FEntityRuntimeEntry>& Pair : EntityEntries)
	{
		if (Pair.Value.State.ObjectType != se::common::OBJ_STORE)
		{
			continue;
		}

		if (AStoreActor* StoreActor = Cast<AStoreActor>(Pair.Value.Actor.Get()))
		{
			OutStoreActors.Add(StoreActor);
		}
	}
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
	if (SpawnedActor == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Network] spawn Failed"));
		return nullptr;
	}
	
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
			UE_LOG(LogTemp, Warning, TEXT("[Rocket] Spawned"));
			
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
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[Network] Spawned projectile actor is not of type ATimeThiefRocketProjectile. Actor=%s"), *GetNameSafe(Actor));
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

	if (UsesSpawnTransformOnly(EntityState.ObjectType))
	{
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
	// if (IsLocalPlayerEntity(EntityId))
	// {
	// 	return;
	// }

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
