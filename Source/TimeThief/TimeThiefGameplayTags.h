#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MorphingMesh/MorphingMeshComponent.h"

class UGameplayTagsManager;

struct FTimeThiefGameplayTags {
public:
	static const FTimeThiefGameplayTags& Get() { return GameplayTags; }
	static void InitializeNativeGameplayTags();
	static uint32 ResolveWeaponIdFromTag(const FGameplayTag& WeaponTag);
	static FGameplayTag ResolveWeaponTagFromId(uint32 WeaponId);
	static EMorphTargetType GetMorphTargetTypeByTag(FGameplayTag WeaponTag);
	
	// Input Tags
	FGameplayTag InputTag_Action_Move;
	FGameplayTag InputTag_Action_Look;
	FGameplayTag InputTag_Action_Jump;
	FGameplayTag InputTag_Action_Fire;
	FGameplayTag InputTag_Action_Reload;
	FGameplayTag InputTag_Action_Aim;
	FGameplayTag InputTag_Action_Melee;
	FGameplayTag InputTag_Action_EquipRifle;
	FGameplayTag InputTag_Action_EquipShotgun;
	FGameplayTag InputTag_Action_EquipRocketLauncher;
	FGameplayTag InputTag_Action_Wire;
	FGameplayTag InputTag_Action_TogglePerspective;
	FGameplayTag InputTag_Action_ToggleMinimap;
	FGameplayTag InputTag_Action_ToggleControlGuide;
	FGameplayTag InputTag_Action_CloseUI;
	FGameplayTag InputTag_Action_Interact;
	FGameplayTag InputTag_Action_Inventory;
	FGameplayTag InputTag_Action_WheelMenu;
	FGameplayTag InputTag_Action_SavePoint;
	FGameplayTag InputTag_Action_Throw;
	FGameplayTag InputTag_Skill_Slot1;
	FGameplayTag InputTag_Skill_Slot2;
	
	FGameplayTag Weapon_Rifle;
	FGameplayTag Weapon_Shotgun;
	FGameplayTag Weapon_RocketLauncher;
	
	FGameplayTag State_Combat_Rifle;
	FGameplayTag State_Combat_Shotgun;
	FGameplayTag State_Combat_RocketLauncher;
	
	FGameplayTag InitState_Spawned;
	FGameplayTag InitState_DataAvailable;
	FGameplayTag InitState_DataInitialized;
	FGameplayTag InitState_GameplayReady;

protected:
	void AddTag(FGameplayTag& OutTag, const ANSICHAR* TagName, const ANSICHAR* TagComment);

private:
	static FTimeThiefGameplayTags GameplayTags;
};
