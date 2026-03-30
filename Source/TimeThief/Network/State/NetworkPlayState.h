#pragma once

#include "CoreMinimal.h"

#include "NetworkPlayState.generated.h"

UENUM(BlueprintType)
enum class ENetworkPlayState : uint8
{
	Disconnected,
	Connected,
	Handshaking,
	InLobby,
	MatchMaking,
	MatchingSucc,
	EnteringRoom,
	InRoom,
	LeavingRoom,
};
