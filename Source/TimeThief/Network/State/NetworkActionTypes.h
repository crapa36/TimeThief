#pragma once

#include "CoreMinimal.h"

#include "NetworkActionTypes.generated.h"

UENUM(BlueprintType)
enum class ENetworkActionType : uint8
{
	None UMETA(DisplayName="None"),
	Jump UMETA(DisplayName="Jump"),
	Crouch UMETA(DisplayName="Crouch"),
	Wire UMETA(DisplayName="Wire"),
	Aim UMETA(DisplayName="Aim"),
};

UENUM(BlueprintType)
enum class ENetworkActionPhase : uint8
{
	None UMETA(DisplayName="None"),
	Start UMETA(DisplayName="Start"),
	End UMETA(DisplayName="End"),
	Land UMETA(DisplayName="Land"),
};

USTRUCT(BlueprintType)
struct FNetworkActionEvent
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly)
	ENetworkActionType ActionType = ENetworkActionType::None;
	
	UPROPERTY(BlueprintReadOnly)
	ENetworkActionPhase Phase = ENetworkActionPhase::None;
	
	// TODO: 필요한 정보가 있다면 더 담기
	// float ServerTime Seconds
	// FVector Event Location
	// ...
	
};