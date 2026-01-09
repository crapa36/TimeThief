// Fill out your copyright notice in the Description page of Project Settings.
#include "Network/NetworkGameInstanceSubsystem.h"

/*---------------------------------
   NetworkGameInstanceSubsystem
---------------------------------*/

void UNetworkGameInstanceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
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

void UNetworkGameInstanceSubsystem::SendPacket(const SendBuffer& Buffer)
{
}

void UNetworkGameInstanceSubsystem::ConnectToServer(const FString& IPAddress, int32 Port)
{
}

void UNetworkGameInstanceSubsystem::SpawnProcessPacketTimer()
{
}

void UNetworkGameInstanceSubsystem::Send(const SendBuffer& Buffer)
{
}

void UNetworkGameInstanceSubsystem::ProcessPacket()
{
}
