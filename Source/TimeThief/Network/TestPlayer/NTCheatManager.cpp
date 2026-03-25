#include "NTCheatManager.h"

#include "Network/NetworkGameInstanceSubsystem.h"

void UNTCheatManager::JoinRoom()
{
	if (auto* NGIS = UNetworkGameInstanceSubsystem::Get(GetWorld()))
	{
		NGIS->RequestEnterRoom();
	}
}

void UNTCheatManager::LeaveRoom()
{
	if (auto* NGIS = UNetworkGameInstanceSubsystem::Get(GetWorld()))
	{
		NGIS->RequestLeaveRoom();
	}
}
