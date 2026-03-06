#include "Generated/ClientPacketHandler.h"
#include "PacketSession.h"

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
	return false;
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

bool Handle_N_RoomReadyChanged(PacketSessionRef& session, const se::room::N_RoomReadyChanged& pkt)
{
	return false;
}

bool Handle_N_GameStart(PacketSessionRef& session, const se::room::N_GameStart& pkt)
{
	return false;
}

bool Handle_S_EntityState(PacketSessionRef& session, const se::room::S_EntityState& pkt)
{
	return false;
}

bool Handle_N_EntitySpawn(PacketSessionRef& session, const se::room::N_EntitySpawn& pkt)
{
	return false;
}

bool Handle_N_EntityDespawn(PacketSessionRef& session, const se::room::N_EntityDespawn& pkt)
{
	return false;
}

bool Handle_N_HitEvent(PacketSessionRef& session, const se::room::N_HitEvent& pkt)
{
	return false;
}

