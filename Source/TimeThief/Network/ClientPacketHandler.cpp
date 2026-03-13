#include "Generated/ClientPacketHandler.h"
#include "PacketSession.h"
#include "Network/NetworkGameInstanceSubsystem.h"

PacketHandlerFunc GPacketHandler[kMaxMessageId + 1];

bool Handle_INVALID(PacketSessionRef& session, BYTE* buffer, int32 len)
{
	return false;
}


bool Handle_S_HandshakeRes(PacketSessionRef& session, const se::auth::S_HandshakeRes& pkt)
{
	return false;
}
bool Handle_S_LoginRes(PacketSessionRef& session, const se::auth::S_LoginRes& pkt)
{
	return false;
}

bool Handle_S_Pong(PacketSessionRef& session, const se::auth::S_Pong& pkt)
{
	return false;
}

bool Handle_S_LobbyEnterRes(PacketSessionRef& session, const se::lobby::S_LobbyEnterRes& pkt)
{
	if (auto* GameInstance = GWorld->GetGameInstance())
	{
		auto* NetworkGameInstance = GameInstance->GetSubsystem<UNetworkGameInstanceSubsystem>();
		
		 NetworkGameInstance->HandleLobbyEnter(pkt);
	}
	
	return true;
}

bool Handle_S_MatchQueueEnterRes(PacketSessionRef& session, const se::lobby::S_MatchQueueEnterRes& pkt)
{
	return false;
}

bool Handle_S_MatchQueueCancelRes(PacketSessionRef& session, const se::lobby::S_MatchQueueCancelRes& pkt)
{
	return false;
}

bool Handle_N_MatchFound(PacketSessionRef& session, const se::lobby::N_MatchFound& pkt)
{
	return false;
}

bool Handle_S_JoinRoom(PacketSessionRef& session, const se::room::S_JoinRoom& pkt)
{
	// 방에 처음 입장한 상태를 업데이트하는 패킷
	// RoomPlayer들이 모두 Spawn 된 상태에서 RoomPlayer들의 스냅샷 정보와, Entity로서 Spawn해야 하는 정보를 담고있다
	
	if (auto* GameInstance = GWorld->GetGameInstance())
	{
		auto* NetworkGameInstance = GameInstance->GetSubsystem<UNetworkGameInstanceSubsystem>();
		
		NetworkGameInstance->HandleJoinRoom(pkt);
	}
	
	return true;
}

bool Handle_N_GameStart(PacketSessionRef& session, const se::room::N_GameStart& pkt)
{
	return false;
}

bool Handle_S_EntityState(PacketSessionRef& session, const se::room::S_EntityState& pkt)
{
	// 다른 여러 Entity의 상태를 업데이트하는 패킷 (본인이 될 수도 있음)
	
	if (auto* GameInstance = GWorld->GetGameInstance())
	{
		auto* NetworkGameInstance = GameInstance->GetSubsystem<UNetworkGameInstanceSubsystem>();
		
		NetworkGameInstance->HandleMove(pkt);
	}
	
	return true;
}

bool Handle_N_EntitySpawn(PacketSessionRef& session, const se::room::N_EntitySpawn& pkt)
{
	// 다른 플레이어 등의 Entity를 생성하는 패킷
	// 기본적으로 self Entity는 예외
	
	if (auto* GameInstance = GWorld->GetGameInstance())
	{
		auto* NetworkGameInstance = GameInstance->GetSubsystem<UNetworkGameInstanceSubsystem>();
		
		NetworkGameInstance->HandleSpawn(pkt);
	}
	
	return true;
}

bool Handle_N_EntityDespawn(PacketSessionRef& session, const se::room::N_EntityDespawn& pkt)
{
	return false;
}

bool Handle_N_HitEvent(PacketSessionRef& session, const se::room::N_HitEvent& pkt)
{
	return false;
}

