#include "TimeThiefGameplayTags.h"
#include "GameplayTagsManager.h"

FTimeThiefGameplayTags FTimeThiefGameplayTags::GameplayTags;

void FTimeThiefGameplayTags::InitializeNativeGameplayTags() {
	GameplayTags.AddTag(GameplayTags.InputTag_Action_Move, "InputTag.Action.Move", "Move Input");
	GameplayTags.AddTag(GameplayTags.InputTag_Action_Look, "InputTag.Action.Look", "Look Input");
	GameplayTags.AddTag(GameplayTags.InputTag_Action_Jump, "InputTag.Action.Jump", "Jump Input");
	GameplayTags.AddTag(GameplayTags.InputTag_Action_Fire, "InputTag.Action.Fire", "Fire Input");
	GameplayTags.AddTag(GameplayTags.InputTag_Action_Reload, "InputTag.Action.Reload", "Reload Input");
	GameplayTags.AddTag(GameplayTags.InputTag_Action_Aim, "InputTag.Action.Aim", "Aim Input");
	GameplayTags.AddTag(GameplayTags.InputTag_Action_Melee, "InputTag.Action.Melee", "Melee Input");
	GameplayTags.AddTag(GameplayTags.InputTag_Action_EquipRifle, "InputTag.Action.EquipRifle", "Equip Rifle Input");

	GameplayTags.AddTag(GameplayTags.Weapon_Rifle, "Weapon.Rifle", "Rifle Weapon Type");
	GameplayTags.AddTag(GameplayTags.Weapon_Pistol, "Weapon.Pistol", "Pistol Weapon Type");


	GameplayTags.AddTag(GameplayTags.State_Combat_Rifle, "State.Combat.Rifle", "Character is holding a Rifle");
	GameplayTags.AddTag(GameplayTags.State_Combat_Pistol, "State.Combat.Pistol", "Character is holding a Pistol");

	GameplayTags.AddTag(GameplayTags.InitState_Spawned, "InitState.Spawned", "Actor has been spawned");
	GameplayTags.AddTag(GameplayTags.InitState_DataAvailable, "InitState.DataAvailable", "Data is available");
	GameplayTags.AddTag(GameplayTags.InitState_DataInitialized, "InitState.DataInitialized", "Data has been initialized");
	GameplayTags.AddTag(GameplayTags.InitState_GameplayReady, "InitState.GameplayReady", "Ready for gameplay");
}

void FTimeThiefGameplayTags::AddTag(FGameplayTag& OutTag, const ANSICHAR* TagName, const ANSICHAR* TagComment) {
	
	OutTag = UGameplayTagsManager::Get().AddNativeGameplayTag(FName(TagName), FString(TagComment));
}