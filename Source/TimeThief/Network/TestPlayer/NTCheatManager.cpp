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
		NGIS->RequestMatchQueueEnter();
	}
}

void UNTCheatManager::CancelMatchQueue()
{
	if (auto* NGIS = UNetworkGameInstanceSubsystem::Get(GetWorld()))
	{
		NGIS->RequestMatchQueueCancel();
	}
}

// void UNTCheatManager::JoinRoom()
// {
// 	if (auto* NGIS = UNetworkGameInstanceSubsystem::Get(GetWorld()))
// 	{
// 		NGIS->RequestEnterRoom();
// 	}
// }
//
// void UNTCheatManager::LeaveRoom()
// {
// 	if (auto* NGIS = UNetworkGameInstanceSubsystem::Get(GetWorld()))
// 	{
// 		NGIS->RequestLeaveRoom();
// 	}
// }

void UNTCheatManager::Pos()
{
	APlayerController* PC = GetOuterAPlayerController();
	if (PC == nullptr) return;
	
	APawn* Pawn = PC->GetPawn();
	if (Pawn == nullptr) return;
	
	const FVector Location = Pawn->GetActorLocation();
	
	UE_LOG(LogTemp, Warning, TEXT("[Cheat] Position: X=%.2f Y=%.2f Z=%.2f"),
		Location.X, Location.Y, Location.Z);
	
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			5.0f,
			FColor::Green,
			FString::Printf(TEXT("Position: X=%.2f Y=%.2f Z=%.2f"),
				Location.X, Location.Y, Location.Z)
		);
	}
}

void UNTCheatManager::Tp(float X, float Y, float Z)
{
	APlayerController* PC = GetOuterAPlayerController();
	if (PC == nullptr)
		return;

	APawn* Pawn = PC->GetPawn();
	if (Pawn == nullptr)
		return;

	const FVector NewLocation(X, Y, Z);

	Pawn->SetActorLocation(NewLocation, false, nullptr, ETeleportType::TeleportPhysics);

	UE_LOG(LogTemp, Warning, TEXT("[Cheat] Teleport: X=%.2f Y=%.2f Z=%.2f"), X, Y, Z);
}

void UNTCheatManager::TpAll(float X, float Y, float Z)
{
	UE_LOG(LogTemp, Warning, TEXT("[Cheat] TpAll: X=%.2f Y=%.2f Z=%.2f"), X, Y, Z);

	if (auto* NGIS = UNetworkGameInstanceSubsystem::Get(GetWorld()))
	{
		NGIS->RequestTPAll(FVector(X, Y, Z));
	}
}

void UNTCheatManager::TestSpawnMonster(float X, float Y, float Z, int32 MonsterType)
{
	if (MonsterType < 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat] Invalid MonsterType: %d"), MonsterType);
		return;
	}
	
	UE_LOG(LogTemp, Warning, TEXT("[Cheat] TestSpawnMonster: X=%.2f Y=%.2f Z=%.2f MonsterType=%d"), X, Y, Z, MonsterType);
	
	if (auto* NGIS = UNetworkGameInstanceSubsystem::Get(GetWorld()))
	{
		NGIS->RequestSpawnMonster(FVector(X, Y, Z), MonsterType);
	}
}

void UNTCheatManager::TestSpawnChest(float X, float Y, float Z)
{
	UE_LOG(LogTemp, Warning, TEXT("[Cheat] TestSpawnChest: X=%.2f Y=%.2f Z=%.2f"), X, Y, Z);
	
	if (auto* NGIS = UNetworkGameInstanceSubsystem::Get(GetWorld()))
	{
		NGIS->RequestSpawnChest(FVector(X, Y, Z));
	}
}

void UNTCheatManager::TestSpawnStore(float X, float Y, float Z)
{
	UE_LOG(LogTemp, Warning, TEXT("[Cheat] TestSpawnStore: X=%.2f Y=%.2f Z=%.2f"), X, Y, Z);
	
	if (auto* NGIS = UNetworkGameInstanceSubsystem::Get(GetWorld()))
	{
		NGIS->RequestSpawnStore(FVector(X, Y, Z));
	}
}

void UNTCheatManager::TestItemReq(int32 ItemId, int32 Amount)
{
	if (ItemId < 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat] Invalid ItemId: %d"), ItemId);
		return;
	}
	
	if (Amount <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat] Invalid Amount: %d"), Amount);
		return;
	}
	
	UE_LOG(LogTemp, Warning, TEXT("[Cheat] TestItemReq: ItemId=%d Amount=%d"), ItemId, Amount);
	
	if (auto* NGIS = UNetworkGameInstanceSubsystem::Get(GetWorld()))
	{
		NGIS->RequestItemReq(ItemId, Amount);
	}
}

void UNTCheatManager::TestMoneyReq(int32 Amount)
{
	if (Amount <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat] Invalid Amount: %d"), Amount);
		return;
	}
	
	UE_LOG(LogTemp, Warning, TEXT("[Cheat] TestMoneyReq: Amount=%d"), Amount);
	
	if (auto* NGIS = UNetworkGameInstanceSubsystem::Get(GetWorld()))
	{
		NGIS->RequestMoneyReq(Amount);
	}
}

void UNTCheatManager::TestHealthReq(int32 Health)
{
	if (Health <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat] Invalid Health: %d"), Health);
		return;
	}
	
	UE_LOG(LogTemp, Warning, TEXT("[Cheat] TestHealthReq: Health=%d"), Health);
	
	if (auto* NGIS = UNetworkGameInstanceSubsystem::Get(GetWorld()))
	{
		NGIS->RequestHealthReq(Health);
	}
}

void UNTCheatManager::TestMaxHealthReq(int32 MaxHealth)
{
	if (MaxHealth <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat] Invalid MaxHealth: %d"), MaxHealth);
		return;
	}
	
	UE_LOG(LogTemp, Warning, TEXT("[Cheat] TestMaxHealthReq: MaxHealth=%d"), MaxHealth);
	
	if (auto* NGIS = UNetworkGameInstanceSubsystem::Get(GetWorld()))
	{
		NGIS->RequestMaxHealthReq(MaxHealth);
	}
}

void UNTCheatManager::TestZoneStop()
{
	UE_LOG(LogTemp, Warning, TEXT("[Cheat] TestZoneStop"));
	
	if (auto* NGIS = UNetworkGameInstanceSubsystem::Get(GetWorld()))
	{
		NGIS->RequestZoneStop();
	}
}

void UNTCheatManager::TestZoneStart()
{
	UE_LOG(LogTemp, Warning, TEXT("[Cheat] TestZoneStart"));
	
	if (auto* NGIS = UNetworkGameInstanceSubsystem::Get(GetWorld()))
	{
		NGIS->RequestZoneStart();
	}
}

void UNTCheatManager::TestZoneReset()
{
	UE_LOG(LogTemp, Warning, TEXT("[Cheat] TestZoneReset"));
	
	if (auto* NGIS = UNetworkGameInstanceSubsystem::Get(GetWorld()))
	{
		NGIS->RequestZoneReset();
	}
}

void UNTCheatManager::TestZoneDamageOff()
{
	UE_LOG(LogTemp, Warning, TEXT("[Cheat] TestZoneDamageOff"));
	
	if (auto* NGIS = UNetworkGameInstanceSubsystem::Get(GetWorld()))
	{
		NGIS->RequestZoneDamageOff();
	}
}

void UNTCheatManager::TestZoneDamageOn()
{
	UE_LOG(LogTemp, Warning, TEXT("[Cheat] TestZoneDamageOn"));
	
	if (auto* NGIS = UNetworkGameInstanceSubsystem::Get(GetWorld()))
	{
		NGIS->RequestZoneDamageOn();
	}
}
