#include "TimeThiefGameplayTags.h"
#include "GameplayTagsManager.h"

FTimeThiefGameplayTags FTimeThiefGameplayTags::GameplayTags;

void FTimeThiefGameplayTags::InitializeNativeGameplayTags()
{
	GameplayTags.AddTag(GameplayTags.InputTag_Action_Move, "InputTag.Action.Move", "Move Input");
	GameplayTags.AddTag(GameplayTags.InputTag_Action_Look, "InputTag.Action.Look", "Look Input");
	GameplayTags.AddTag(GameplayTags.InputTag_Action_Jump, "InputTag.Action.Jump", "Jump Input");
	GameplayTags.AddTag(GameplayTags.InputTag_Action_Fire, "InputTag.Action.Fire", "Fire Input");
	GameplayTags.AddTag(GameplayTags.InputTag_Action_Reload, "InputTag.Action.Reload", "Reload Input");
	GameplayTags.AddTag(GameplayTags.InputTag_Action_Aim, "InputTag.Action.Aim", "Aim Input");
	GameplayTags.AddTag(GameplayTags.InputTag_Action_Melee, "InputTag.Action.Melee", "Melee Input");
	GameplayTags.AddTag(GameplayTags.InputTag_Action_EquipRifle, "InputTag.Action.EquipRifle", "Equip Rifle Input");
	GameplayTags.AddTag(GameplayTags.InputTag_Action_EquipShotgun, "InputTag.Action.EquipShotgun", "Equip Shotgun Input");
	GameplayTags.AddTag(GameplayTags.InputTag_Action_EquipRocketLauncher, "InputTag.Action.EquipRocketLauncher", "Equip RocketLauncher Input");
	GameplayTags.AddTag(GameplayTags.InputTag_Action_Wire, "InputTag.Action.Wire", "Wire Action Input");
	GameplayTags.AddTag(GameplayTags.InputTag_Action_TogglePerspective, "InputTag.Action.TogglePerspective", "Toggle Perspective Input");
	GameplayTags.AddTag(GameplayTags.InputTag_Action_ToggleMinimap, "InputTag.Action.ToggleMinimap", "Toggle Show Minimap Input");
	GameplayTags.AddTag(GameplayTags.InputTag_Action_Interact, "InputTag.Action.Interact", "Interact Input");
	GameplayTags.AddTag(GameplayTags.InputTag_Action_Inventory, "InputTag.Action.Inventory", "Inventory Interaction Input");
	GameplayTags.AddTag(GameplayTags.InputTag_Action_WheelMenu, "InputTag.Action.WheelMenu", "Use Item Wheel Menu Input");
	GameplayTags.AddTag(GameplayTags.InputTag_Action_SavePoint, "InputTag.Action.SavePoint", "Save Point Input");
	
	GameplayTags.AddTag(GameplayTags.Weapon_Rifle, "Weapon.Rifle", "Rifle Weapon Type");
	GameplayTags.AddTag(GameplayTags.Weapon_Shotgun, "Weapon.Shotgun", "Shotgun Weapon Type");
	GameplayTags.AddTag(GameplayTags.Weapon_RocketLauncher, "Weapon.RocketLauncher", "RocketLauncher Weapon Type");
	
	GameplayTags.AddTag(GameplayTags.State_Combat_Rifle, "State.Combat.Rifle", "Character is holding a Rifle");
	GameplayTags.AddTag(GameplayTags.State_Combat_Shotgun, "State.Combat.Shotgun", "Character is holding a Shotgun");
	GameplayTags.AddTag(GameplayTags.State_Combat_RocketLauncher, "State.Combat.RocketLauncher", "Character is holding a RocketLauncher");
	
	GameplayTags.AddTag(GameplayTags.InitState_Spawned, "InitState.Spawned", "Actor has been spawned");
	GameplayTags.AddTag(GameplayTags.InitState_DataAvailable, "InitState.DataAvailable", "Data is available");
	GameplayTags.AddTag(GameplayTags.InitState_DataInitialized, "InitState.DataInitialized", "Data has been initialized");
	GameplayTags.AddTag(GameplayTags.InitState_GameplayReady, "InitState.GameplayReady", "Ready for gameplay");
}

uint32 FTimeThiefGameplayTags::ResolveWeaponIdFromTag(const FGameplayTag& WeaponTag)
{
	const FTimeThiefGameplayTags& Tags = Get();
	if (WeaponTag == Tags.Weapon_Rifle)
	{
		return 1;
	}
	if (WeaponTag == Tags.Weapon_Shotgun)
	{
		return 2;
	}
	if (WeaponTag == Tags.Weapon_RocketLauncher)
	{
		return 3;
	}
	return 0;
}

FGameplayTag FTimeThiefGameplayTags::ResolveWeaponTagFromId(uint32 WeaponId)
{
	const FTimeThiefGameplayTags& Tags = Get();
	switch (WeaponId)
	{
	case 1:
		return Tags.Weapon_Rifle;
	case 2:
		return Tags.Weapon_Shotgun;
	case 3:
		return Tags.Weapon_RocketLauncher;
	default:
		return FGameplayTag();
	}
}

EMorphTargetType FTimeThiefGameplayTags::GetMorphTargetTypeByTag(FGameplayTag WeaponTag)
{
	if (WeaponTag == GameplayTags.Weapon_Rifle)
	{
		return EMorphTargetType::A;
	}
	if (WeaponTag == GameplayTags.Weapon_Shotgun)
	{
		return EMorphTargetType::B;
	}
	if (WeaponTag == GameplayTags.Weapon_RocketLauncher)
	{
		return EMorphTargetType::C;
	}
	return EMorphTargetType::None;
}

void FTimeThiefGameplayTags::AddTag(FGameplayTag& OutTag, const ANSICHAR* TagName, const ANSICHAR* TagComment)
{
	OutTag = UGameplayTagsManager::Get().AddNativeGameplayTag(FName(TagName), FString(TagComment));
}
