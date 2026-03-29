#include "NTCheatManager.h"

#include "Network/NetworkGameInstanceSubsystem.h"

void UNTCheatManager::SetNickname(const FString& Nickname)
{
	if (auto* NGIS = UNetworkGameInstanceSubsystem::Get(GetWorld()))
	{
		NGIS->RequestSetNickname(Nickname);
	}
}

void UNTCheatManager::EnterMatchQueue()
{
	if (auto* NGIS = UNetworkGameInstanceSubsystem::Get(GetWorld()))
	{
		NGIS->RequestEnterRoom();
	}
}

void UNTCheatManager::CancelMatchQueue()
{
	if (auto* NGIS = UNetworkGameInstanceSubsystem::Get(GetWorld()))
	{
		NGIS->RequestLeaveRoom();
	}
}

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
