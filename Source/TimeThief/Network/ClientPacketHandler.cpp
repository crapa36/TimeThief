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
	if (!session)
		return false;
	
	if (UGameInstance* GI = GWorld ? GWorld->GetGameInstance() : nullptr)
	{
		if (UNetworkGameInstanceSubsystem* NGIS = GI->GetSubsystem<UNetworkGameInstanceSubsystem>()) 
		{
			NGIS->HandleHandshakeRes(pkt);
			return true;
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("Handle_S_HandshakeRes: Failed to get NGIS"));
	return false;
}

bool Handle_S_LoginRes(PacketSessionRef& session, const se::auth::S_LoginRes& pkt)
{
	return false;
}

bool Handle_S_Pong(PacketSessionRef& session, const se::auth::S_Pong& pkt)
{
	if (!session)
		return false;
	
	if (UGameInstance* GI = GWorld ? GWorld->GetGameInstance() : nullptr)
	{
		if (UNetworkGameInstanceSubsystem* NGIS = GI->GetSubsystem<UNetworkGameInstanceSubsystem>()) 
		{
			NGIS->HandlePong(pkt);
			return true;
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("Handle_S_Pong: Failed to get NGIS"));
	return false;	
}
	
bool Handle_S_SetNicknameRes(PacketSessionRef& session, const se::lobby::S_SetNicknameRes& pkt)
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
	
bool Handle_S_RoomEnterRes(PacketSessionRef& session, const se::room::S_RoomEnterRes& pkt)
{
	if (!session)
		return false;
	
	if (UGameInstance* GI = GWorld ? GWorld->GetGameInstance() : nullptr)
	{
		if (UNetworkGameInstanceSubsystem* NGIS = GI->GetSubsystem<UNetworkGameInstanceSubsystem>()) 
		{
			NGIS->HandleRoomEnterRes(pkt);
			return true;
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("Handle_S_RoomEnterRes: Failed to get NGIS"));
	return false;
}
	
bool Handle_S_RoomLeaveRes(PacketSessionRef& session, const se::room::S_RoomLeaveRes& pkt)
{
	if (!session)
		return false;
	
	if (UGameInstance* GI = GWorld ? GWorld->GetGameInstance() : nullptr)
	{
		if (UNetworkGameInstanceSubsystem* NGIS = GI->GetSubsystem<UNetworkGameInstanceSubsystem>()) 
		{
			NGIS->HandleRoomLeaveRes(pkt);
			return true;
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("Handle_S_RoomLeaveRes: Failed to get NGIS"));
	return false;
}
	
bool Handle_N_EntitySpawn(PacketSessionRef& session, const se::room::N_EntitySpawn& pkt)
{
	if (!session)
		return false;
	
	if (!pkt.has_info())
	{
		UE_LOG(LogTemp, Warning, TEXT("Handle_N_EntitySpawn: pkt has no entity"));
		return false;
	}
	
	const auto& Info = pkt.info();
	
	if (!Info.has_entity_id())
	{
		UE_LOG(LogTemp, Warning, TEXT("Handle_N_EntitySpawn: entity has no entity_id"));
		return false;
	}
	
	if (!Info.has_movement())
	{
		UE_LOG(LogTemp, Warning, TEXT("Handle_N_EntitySpawn: entity has no movement info"));
		return false;
	}
	
	if (Info.entity_id().value() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Handle_N_EntitySpawn: entity_id is 0"));
		return false;
	}
	
	if (UGameInstance* GI = GWorld ? GWorld->GetGameInstance() : nullptr)
	{
		if (UNetworkGameInstanceSubsystem* NGIS = GI->GetSubsystem<UNetworkGameInstanceSubsystem>()) 
		{
			NGIS->HandleEntitySpawn(pkt);
			return true;
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("Handle_N_EntitySpawn: Failed to get NGIS"));
	return false;
}
	
bool Handle_N_EntityDespawn(PacketSessionRef& session, const se::room::N_EntityDespawn& pkt)
{
	if (!session)
		return false;
	
	if (!pkt.has_entity_id())
	{
		UE_LOG(LogTemp, Warning, TEXT("Handle_N_EntityDespawn: pkt has no entity_id"));
		return false;
	}
	
	if (pkt.entity_id().value() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Handle_N_EntityDespawn: entity_id is 0"));
		return false;
	}
	
	if (UGameInstance* GI = GWorld ? GWorld->GetGameInstance() : nullptr)
	{
		if (UNetworkGameInstanceSubsystem* NGIS = GI->GetSubsystem<UNetworkGameInstanceSubsystem>()) 
		{
			NGIS->HandleEntityDespawn(pkt);
			return true;
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("Handle_N_EntityDespawn: Failed to get NGIS"));
	return false;	
}
	
bool Handle_N_RoomClosed(PacketSessionRef& session, const se::room::N_RoomClosed& pkt)
{
	return false;	
}
	
bool Handle_N_GameStart(PacketSessionRef& session, const se::game::N_GameStart& pkt)
{
	return false;	
}
	
bool Handle_N_GameEnd(PacketSessionRef& session, const se::game::N_GameEnd& pkt)
{
	return false;	
}
	
bool Handle_N_Move(PacketSessionRef& session, const se::game::N_Move& pkt)
{
	if (!session)
		return false;
	
	if (!pkt.has_entity_id())
		return false;
	
	if (pkt.entity_id().value() == 0)
		return false;
	
	if (!pkt.has_movement())
		return false;
	
	if (UGameInstance* GI = GWorld ? GWorld->GetGameInstance() : nullptr)
	{
		if (UNetworkGameInstanceSubsystem* NGIS = GI->GetSubsystem<UNetworkGameInstanceSubsystem>()) 
		{
			NGIS->HandleMove(pkt);
			return true;
		}
	}
	
	// No Log here because movement packets are very frequent
	return false;	
}
	
bool Handle_N_Fire(PacketSessionRef& session, const se::game::N_Fire& pkt)
{
	return false;	
}
	
bool Handle_N_Attack(PacketSessionRef& session, const se::game::N_Attack& pkt)
{
	return false;	
}
	
bool Handle_N_ThrowGrenade(PacketSessionRef& session, const se::game::N_ThrowGrenade& pkt)
{
	return false;	
}
	
bool Handle_N_Reload(PacketSessionRef& session, const se::game::N_Reload& pkt)
{
	return false;	
}
	
bool Handle_N_WeaponChanged(PacketSessionRef& session, const se::game::N_WeaponChanged& pkt)
{
	return false;	
}
	
bool Handle_N_UseAbility(PacketSessionRef& session, const se::game::N_UseAbility& pkt)
{
	return false;	
}
	
bool Handle_N_KillPlayer(PacketSessionRef& session, const se::game::N_KillPlayer& pkt)
{
	return false;	
}
	
bool Handle_N_UseItem(PacketSessionRef& session, const se::game::N_UseItem& pkt)
{
	return false;	
}
	
bool Handle_N_PickupItem(PacketSessionRef& session, const se::game::N_PickupItem& pkt)
{
	return false;	
}
	
bool Handle_S_UseStoreRes(PacketSessionRef& session, const se::game::S_UseStoreRes& pkt)
{
	return false;	
}
	
bool Handle_N_ItemGained(PacketSessionRef& session, const se::game::N_ItemGained& pkt)
{
	return false;	
}
	
bool Handle_N_HealthChanged(PacketSessionRef& session, const se::game::N_HealthChanged& pkt)
{
	return false;	
}
	
bool Handle_N_EntityDied(PacketSessionRef& session, const se::game::N_EntityDied& pkt)
{
	return false;	
}
	
bool Handle_N_EntityRespawned(PacketSessionRef& session, const se::game::N_EntityRespawned& pkt)
{
	return false;	
}
	
bool Handle_N_EntityDestroyed(PacketSessionRef& session, const se::game::N_EntityDestroyed& pkt)
{
	return false;	
}
	
bool Handle_N_TimePointChanged(PacketSessionRef& session, const se::game::N_TimePointChanged& pkt)
{
	return false;	
}
	
bool Handle_N_TimeStormChange(PacketSessionRef& session, const se::game::N_TimeStormChange& pkt)
{
	return false;	
}
