#include "Generated/ClientPacketHandler.h"
#include "PacketSession.h"

PacketHandlerFunc GPacketHandler[UINT16_MAX];

bool Handle_INVALID(PacketSessionRef& session, BYTE* buffer, int32 len)
{
	return false;
}


bool Handle_S_HandshakeRes(PacketSessionRef& session, BYTE* buffer, int32 len)
{
	return false;
}
bool Handle_S_LoginRes(PacketSessionRef& session, BYTE* buffer, int32 len)
{
	return false;
}

bool Handle_S_Pong(PacketSessionRef& session, BYTE* buffer, int32 len)
{
	return false;
}

bool Handle_S_LobbyEnterRes(PacketSessionRef& session, BYTE* buffer, int32 len)
{
	return false;
}

bool Handle_S_MatchQueueEnterRes(PacketSessionRef& session, BYTE* buffer, int32 len)
{
	return false;
}

bool Handle_S_MatchQueueCancelRes(PacketSessionRef& session, BYTE* buffer, int32 len)
{
	return false;
}

bool Handle_N_MatchFound(PacketSessionRef& session, BYTE* buffer, int32 len)
{
	return false;
}

bool Handle_N_RoomReadyChanged(PacketSessionRef& session, BYTE* buffer, int32 len)
{
	return false;
}

bool Handle_N_GameStart(PacketSessionRef& session, BYTE* buffer, int32 len)
{
	return false;
}

bool Handle_S_EntityState(PacketSessionRef& session, BYTE* buffer, int32 len)
{
	return false;
}

bool Handle_N_EntitySpawn(PacketSessionRef& session, BYTE* buffer, int32 len)
{
	return false;
}

bool Handle_N_EntityDespawn(PacketSessionRef& session, BYTE* buffer, int32 len)
{
	return false;
}

bool Handle_N_HitEvent(PacketSessionRef& session, BYTE* buffer, int32 len)
{
	return false;
}

