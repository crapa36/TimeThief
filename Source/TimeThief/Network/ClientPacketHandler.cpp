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
	if (!session)
		return false;
	
	if (UGameInstance* GI = GWorld ? GWorld->GetGameInstance() : nullptr)
	{
		if (UNetworkGameInstanceSubsystem* NGIS = GI->GetSubsystem<UNetworkGameInstanceSubsystem>()) 
		{
			NGIS->HandleSetNicknameRes(pkt);
			return true;
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("Handle_S_SetNicknameRes: Failed to get NGIS"));
	return false;	
}
	
bool Handle_S_MatchQueueEnterRes(PacketSessionRef& session, const se::lobby::S_MatchQueueEnterRes& pkt)
{
	if (!session)
		return false;
	
	if (UGameInstance* GI = GWorld ? GWorld->GetGameInstance() : nullptr)
	{
		if (UNetworkGameInstanceSubsystem* NGIS = GI->GetSubsystem<UNetworkGameInstanceSubsystem>()) 
		{
			NGIS->HandleMatchQueueEnterRes(pkt);
			return true;
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("Handle_S_MatchQueueEnterRes: Failed to get NGIS"));
	return false;	
}
	
bool Handle_S_MatchQueueCancelRes(PacketSessionRef& session, const se::lobby::S_MatchQueueCancelRes& pkt)
{
	if (!session)
		return false;
	
	if (UGameInstance* GI = GWorld ? GWorld->GetGameInstance() : nullptr)
	{
		if (UNetworkGameInstanceSubsystem* NGIS = GI->GetSubsystem<UNetworkGameInstanceSubsystem>()) 
		{
			NGIS->HandleMatchQueueCancelRes(pkt);
			return true;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("Handle_S_MatchQueueCancelRes: Failed to get NGIS"));
	return false;	
}
	
bool Handle_N_MatchFound(PacketSessionRef& session, const se::lobby::N_MatchFound& pkt)
{
	if (!session)
		return false;
	
	if (UGameInstance* GI = GWorld ? GWorld->GetGameInstance() : nullptr)
	{
		if (UNetworkGameInstanceSubsystem* NGIS = GI->GetSubsystem<UNetworkGameInstanceSubsystem>()) 
		{
			NGIS->HandleMatchFound(pkt);
			return true;
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("Handle_N_MatchFound: Failed to get NGIS"));
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

bool Handle_S_RoomSetupEnd(PacketSessionRef& session, const se::room::S_RoomSetupEnd& pkt)
{
	return false;
}

bool Handle_N_EntitiesSpawn(PacketSessionRef& session, const se::room::N_EntitiesSpawn& pkt)
{
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

bool Handle_N_Jump(PacketSessionRef& session, const se::game::N_Jump& pkt)
{
	if (!session)
		return false;

	if (!pkt.has_entity_id() || pkt.entity_id().value() == 0)
		return false;

	if (UGameInstance* GI = GWorld ? GWorld->GetGameInstance() : nullptr)
	{
		if (UNetworkGameInstanceSubsystem* NGIS = GI->GetSubsystem<UNetworkGameInstanceSubsystem>())
		{
			NGIS->HandleJump(pkt);
			return true;
		}
	}

	return false;
}
	
bool Handle_N_JumpLand(PacketSessionRef& session, const se::game::N_JumpLand& pkt)
{
	if (!session)
		return false;

	if (!pkt.has_entity_id() || pkt.entity_id().value() == 0)
		return false;

	if (UGameInstance* GI = GWorld ? GWorld->GetGameInstance() : nullptr)
	{
		if (UNetworkGameInstanceSubsystem* NGIS = GI->GetSubsystem<UNetworkGameInstanceSubsystem>())
		{
			NGIS->HandleJumpLand(pkt);
			return true;
		}
	}

	return false;
}

bool Handle_N_DoubleJump(PacketSessionRef& session, const se::game::N_DoubleJump& pkt)
{
	return false;
}
	
bool Handle_N_Crouch(PacketSessionRef& session, const se::game::N_Crouch& pkt)
{
	if (!session)
		return false;
	
	if (!pkt.has_entity_id() || pkt.entity_id().value() == 0)
		return false;
	
	if (UGameInstance* GI = GWorld ? GWorld->GetGameInstance() : nullptr)
	{
		if (UNetworkGameInstanceSubsystem* NGIS = GI->GetSubsystem<UNetworkGameInstanceSubsystem>()) 
		{
			NGIS->HandleCrouch(pkt);
			return true;
		}
	}
	
	return false;	
}
	
bool Handle_N_WireAction(PacketSessionRef& session, const se::game::N_WireAction& pkt)
{
	if (!session)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WirePkt][Stage=Recv][N_WireAction] session is null"));
		return false;
	}

	if (!pkt.has_entity_id() || pkt.entity_id().value() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WirePkt][Stage=Recv][N_WireAction] invalid entity_id"));
		return false;
	}

	if (!pkt.has_anchor_point())
	{
		UE_LOG(LogTemp, Warning, TEXT("[WirePkt][Stage=Recv][N_WireAction] missing anchor_point entity=%u"), pkt.entity_id().value());
		return false;
	}

	const auto& Anchor = pkt.anchor_point();
	UE_LOG(LogTemp, Log,
		TEXT("[WirePkt][Stage=Recv][N_WireAction] EntityId=%u Anchor=(%.1f, %.1f, %.1f)"),
		pkt.entity_id().value(),
		Anchor.x(),
		Anchor.y(),
		Anchor.z());

	if (UGameInstance* GI = GWorld ? GWorld->GetGameInstance() : nullptr)
	{
		if (UNetworkGameInstanceSubsystem* NGIS = GI->GetSubsystem<UNetworkGameInstanceSubsystem>())
		{
			NGIS->HandleWireAction(pkt);
			return true;
		}
	}

	return false;
}
	
bool Handle_N_WireActionEnd(PacketSessionRef& session, const se::game::N_WireActionEnd& pkt)
{
	if (!session)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WirePkt][Stage=Recv][N_WireActionEnd] session is null"));
		return false;
	}

	if (!pkt.has_entity_id() || pkt.entity_id().value() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WirePkt][Stage=Recv][N_WireActionEnd] invalid entity_id"));
		return false;
	}

	UE_LOG(LogTemp, Log,
		TEXT("[WirePkt][Stage=Recv][N_WireActionEnd] EntityId=%u"),
		pkt.entity_id().value());

	if (UGameInstance* GI = GWorld ? GWorld->GetGameInstance() : nullptr)
	{
		if (UNetworkGameInstanceSubsystem* NGIS = GI->GetSubsystem<UNetworkGameInstanceSubsystem>())
		{
			NGIS->HandleWireActionEnd(pkt);
			return true;
		}
	}

	return false;
}

bool Handle_N_WireLaunch(PacketSessionRef& session, const se::game::N_WireLaunch& pkt)
{
	if (!session)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WirePkt][Stage=Recv][N_WireLaunch] session is null"));
		return false;
	}

	if (!pkt.has_entity_id() || pkt.entity_id().value() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WirePkt][Stage=Recv][N_WireLaunch] invalid entity_id"));
		return false;
	}

	if (!pkt.has_start_position() || !pkt.has_direction())
	{
		UE_LOG(LogTemp, Warning, TEXT("[WirePkt][Stage=Recv][N_WireLaunch] missing start_position or direction entity=%u"), pkt.entity_id().value());
		return false;
	}

	const auto& Start = pkt.start_position();
	const auto& Direction = pkt.direction();
	UE_LOG(LogTemp, Log,
		TEXT("[WirePkt][Stage=Recv][N_WireLaunch] EntityId=%u Start=(%.1f, %.1f, %.1f) Dir=(%.2f, %.2f, %.2f)"),
		pkt.entity_id().value(),
		Start.x(),
		Start.y(),
		Start.z(),
		Direction.x(),
		Direction.y(),
		Direction.z());

	if (UGameInstance* GI = GWorld ? GWorld->GetGameInstance() : nullptr)
	{
		if (UNetworkGameInstanceSubsystem* NGIS = GI->GetSubsystem<UNetworkGameInstanceSubsystem>())
		{
			NGIS->HandleWireLaunch(pkt);
			return true;
		}
	}

	return false;	
}
	
bool Handle_N_Aim(PacketSessionRef& session, const se::game::N_Aim& pkt)
{
	if (!session)
		return false;
	
	if (!pkt.has_entity_id())
	{
		UE_LOG(LogTemp, Warning, TEXT("Handle_N_Aim: pkt has no entity_id"));
		return false;
	}
	
	if (pkt.entity_id().value() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Handle_N_Aim: entity_id is 0"));
		return false;
	}
	
	if (UGameInstance* GI = GWorld ? GWorld->GetGameInstance() : nullptr)
	{
		if (UNetworkGameInstanceSubsystem* NGIS = GI->GetSubsystem<UNetworkGameInstanceSubsystem>()) 
		{
			NGIS->HandleAim(pkt);
			return true;
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("Handle_N_Aim: Failed to get NGIS"));
	return false;	
}
	
bool Handle_N_Fire(PacketSessionRef& session, const se::game::N_Fire& pkt)
{
	if (!session)
		return false;
	
	if (!pkt.has_entity_id())
	{
		UE_LOG(LogTemp, Warning, TEXT("Handle_N_Fire: pkt has no entity_id"));
		return false;
	}
	
	if (pkt.entity_id().value() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Handle_N_Fire: entity_id is 0"));
		return false;
	}
	
	if (!pkt.has_start_position())
	{
		UE_LOG(LogTemp, Warning, TEXT("Handle_N_Fire: pkt has no start_position"));
		return false;
	}
	
	if (!pkt.has_direction())
	{
		UE_LOG(LogTemp, Warning, TEXT("Handle_N_Fire: pkt has no direction"));
		return false;
	}
	
	if (UGameInstance* GI = GWorld ? GWorld->GetGameInstance() : nullptr)
	{
		if (UNetworkGameInstanceSubsystem* NGIS = GI->GetSubsystem<UNetworkGameInstanceSubsystem>()) 
		{
			NGIS->HandleFire(pkt);
			return true;
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("Handle_N_Fire: Failed to get NGIS"));
	return false;	
}
	
bool Handle_N_Attack(PacketSessionRef& session, const se::game::N_Attack& pkt)
{
	return false;	
}
	
bool Handle_N_ThrowGrenade(PacketSessionRef& session, const se::game::N_ThrowGrenade& pkt)
{
	if (!session)
		return false;
	
	if (!pkt.has_entity_id())
	{
		UE_LOG(LogTemp, Warning, TEXT("Handle_N_ThrowGrenade: pkt has no entity_id"));
		return false;
	}
	
	if (pkt.entity_id().value() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Handle_N_ThrowGrenade: entity_id is 0"));
		return false;
	}
	
	if (!pkt.has_start_position())
	{
		UE_LOG(LogTemp, Warning, TEXT("Handle_N_ThrowGrenade: pkt has no start_position"));
		return false;
	}
	
	if (!pkt.has_direction())
	{
		UE_LOG(LogTemp, Warning, TEXT("Handle_N_ThrowGrenade: pkt has no direction"));
		return false;
	}
	
	if (UGameInstance* GI = GWorld ? GWorld->GetGameInstance() : nullptr)
	{
		if (UNetworkGameInstanceSubsystem* NGIS = GI->GetSubsystem<UNetworkGameInstanceSubsystem>()) 
		{
			NGIS->HandleThrowGrenade(pkt);
			return true;
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("Handle_N_ThrowGrenade: Failed to get NGIS"));
	return false;
}
	
bool Handle_N_Reload(PacketSessionRef& session, const se::game::N_Reload& pkt)
{
	if (!session)
		return false;
	
	if (!pkt.has_entity_id())
	{
		UE_LOG(LogTemp, Warning, TEXT("Handle_N_Reload: pkt has no entity_id"));
		return false;
	}
	
	if (pkt.entity_id().value() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Handle_N_Reload: entity_id is 0"));
		return false;
	}
	
	if (auto GI = GWorld ? GWorld->GetGameInstance() : nullptr)
	{
		if (auto NGIS = GI->GetSubsystem<UNetworkGameInstanceSubsystem>()) 
		{
			NGIS->HandleReload(pkt);
			return true;
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("Handle_N_Reload: Failed to get NGIS"));
	return false;	
}
	
bool Handle_N_WeaponChanged(PacketSessionRef& session, const se::game::N_WeaponChanged& pkt)
{
	if (!session)
		return false;
	
	if (!pkt.has_entity_id())
	{
		UE_LOG(LogTemp, Warning, TEXT("Handle_N_WeaponChanged: pkt has no entity_id"));
		return false;
	}
	
	if (pkt.entity_id().value() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Handle_N_WeaponChanged: entity_id is 0"));
		return false;
	}
	
	if (auto GI = GWorld ? GWorld->GetGameInstance() : nullptr)
	{
		if (auto NGIS = GI->GetSubsystem<UNetworkGameInstanceSubsystem>()) 
		{
			NGIS->HandleWeaponChanged(pkt);
			return true;
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("Handle_N_WeaponChanged: Failed to get NGIS"));
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

bool Handle_S_ReloadRes(PacketSessionRef& session, const se::game::S_ReloadRes& pkt)
{
	return false;
}

bool Handle_N_EntityHit(PacketSessionRef& session, const se::game::N_EntityHit& pkt)
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

bool Handle_N_ChestInteracted(PacketSessionRef& session, const se::game::N_ChestInteracted& pkt)
{
	return false;	
}

bool Handle_N_ItemLost(PacketSessionRef& session, const se::game::N_ItemLost& pkt)
{
	return false;
}

bool Handle_S_UseItemRes(PacketSessionRef& session, const se::game::S_UseItemRes& pkt)
{
	return false;
}

bool Handle_S_SetSavePointRes(PacketSessionRef& session, const se::game::S_SetSavePointRes& pkt)
{
	if (!session)
		return false;
	
	if (auto GI = GWorld ? GWorld->GetGameInstance() : nullptr)
	{
		if (auto NGIS = GI->GetSubsystem<UNetworkGameInstanceSubsystem>()) 
		{
			NGIS->HandleSetSavePointRes(pkt);
			return true;
		}
	}
	
	return false;
}
	
bool Handle_N_HealthChanged(PacketSessionRef& session, const se::game::N_HealthChanged& pkt)
{
	if (!session)
		return false;
	
	if (!pkt.has_entity_id())
	{
		UE_LOG(LogTemp, Warning, TEXT("Handle_N_HealthChanged: pkt has no entity_id"));
		return false;
	}
	
	if (pkt.entity_id().value() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Handle_N_HealthChanged: entity_id is 0"));
		return false;
	}
	
	if (auto GI = GWorld ? GWorld->GetGameInstance() : nullptr)
	{
		if (auto NGIS = GI->GetSubsystem<UNetworkGameInstanceSubsystem>()) 
		{
			NGIS->HandleHealthChanged(pkt);
			return true;
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("Handle_N_HealthChanged: Failed to get NGIS"));
	return false;	
}
	
bool Handle_N_EntityDied(PacketSessionRef& session, const se::game::N_EntityDied& pkt)
{
	if (!session)
		return false;
	
	if (!pkt.has_entity_id())
	{
		UE_LOG(LogTemp, Warning, TEXT("Handle_N_EntityDied: pkt has no entity_id"));
		return false;
	}
	
	if (pkt.entity_id().value() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Handle_N_EntityDied: entity_id is 0"));
		return false;
	}
	
	if (auto GI = GWorld ? GWorld->GetGameInstance() : nullptr)
	{
		if (auto NGIS = GI->GetSubsystem<UNetworkGameInstanceSubsystem>()) 
		{
			NGIS->HandleEntityDied(pkt);
			return true;
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("Handle_N_EntityDied: Failed to get NGIS"));
	return false;	
}
	
bool Handle_N_EntityRespawned(PacketSessionRef& session, const se::game::N_EntityRespawned& pkt)
{
	UE_LOG(LogTemp, Log, TEXT("Handle_N_EntityRespawned: Received pkt"));
	
	if (!session)
		return false;
	
	if (!pkt.has_entity_id())
	{
		UE_LOG(LogTemp, Warning, TEXT("Handle_N_EntityRespawned: pkt has no entity_id"));
		return false;
	}
	
	if (pkt.entity_id().value() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Handle_N_EntityRespawned: entity_id is 0"));
		return false;
	}
	
	if (!pkt.has_transform())
	{
		UE_LOG(LogTemp, Warning, TEXT("Handle_N_EntityRespawned: pkt has no transform"));
		return false;
	}
	
	if (auto GI = GWorld ? GWorld->GetGameInstance() : nullptr)
	{
		if (auto NGIS = GI->GetSubsystem<UNetworkGameInstanceSubsystem>()) 
		{
			NGIS->HandleEntityRespawned(pkt);
			return true;
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("Handle_N_EntityRespawned: Failed to get NGIS"));
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

bool Handle_N_MaxHealthChanged(PacketSessionRef& session, const se::game::N_MaxHealthChanged& pkt)
{
	return false;
}
	
bool Handle_N_TimeStormChange(PacketSessionRef& session, const se::game::N_TimeStormChange& pkt)
{
	if (!session)
		return false;
	
	if (!pkt.has_center())
	{
		UE_LOG(LogTemp, Warning, TEXT("Handle_N_TimeStormChange: pkt has no center"));
		return false;
	}
	
	if (auto GI = GWorld ? GWorld->GetGameInstance() : nullptr)
	{
		if (auto NGIS = GI->GetSubsystem<UNetworkGameInstanceSubsystem>()) 
		{
			NGIS->HandleTimeStormChange(pkt);
			return true;
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("Handle_N_TimeStormChange: Failed to get NGIS"));
	return false;	
}
