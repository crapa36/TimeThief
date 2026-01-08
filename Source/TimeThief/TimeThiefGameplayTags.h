#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class UGameplayTagsManager;

struct FTimeThiefGameplayTags {
public:
	static const FTimeThiefGameplayTags& Get() { return GameplayTags; }
	static void InitializeNativeGameplayTags();

	// Input Tags
	FGameplayTag InputTag_Action_Move;
	FGameplayTag InputTag_Action_Look;
	FGameplayTag InputTag_Action_Jump;
	FGameplayTag InputTag_Action_Fire;
	FGameplayTag InputTag_Action_Reload;
	FGameplayTag InputTag_Action_Aim;
	FGameplayTag InputTag_Action_Melee;
	FGameplayTag InputTag_Action_EquipRifle;
	FGameplayTag InputTag_Action_Wire;

	FGameplayTag Weapon_Rifle;
	FGameplayTag Weapon_Pistol;

	FGameplayTag State_Combat_Rifle;
	FGameplayTag State_Combat_Pistol;

	FGameplayTag InitState_Spawned;
	FGameplayTag InitState_DataAvailable;
	FGameplayTag InitState_DataInitialized;
	FGameplayTag InitState_GameplayReady;

protected:
	void AddTag(FGameplayTag& OutTag, const ANSICHAR* TagName, const ANSICHAR* TagComment);

private:
	static FTimeThiefGameplayTags GameplayTags;
};