#pragma once

UENUM(BlueprintType)
enum class EMonsterVisualState : uint8
{
	Alive			UMETA(DisplayName = "Alive"),
	Dying			UMETA(DisplayName = "Dying"),
	DeadHidden		UMETA(DisplayName = "DeadHidden"),
	Respawning		UMETA(DisplayName = "Respawning"),
};