#include "TimeThiefGameplayTags.h"
#include "GameplayTagsManager.h"

FTimeThiefGameplayTags FTimeThiefGameplayTags::GameplayTags;

void FTimeThiefGameplayTags::InitializeNativeGameplayTags() {
	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

	GameplayTags.AddTag(GameplayTags.InputTag_Action_Move, "InputTag.Action.Move", "Move Input");
	GameplayTags.AddTag(GameplayTags.InputTag_Action_Look, "InputTag.Action.Look", "Look Input");
	GameplayTags.AddTag(GameplayTags.InputTag_Action_Jump, "InputTag.Action.Jump", "Jump Input");

	GameplayTags.AddTag(GameplayTags.Weapon_Rifle, "Weapon.Rifle", "Rifle Weapon Type");
	GameplayTags.AddTag(GameplayTags.Weapon_Pistol, "Weapon.Pistol", "Pistol Weapon Type");

	GameplayTags.AddTag(GameplayTags.Ability_Weapon_Equip, "Ability.Weapon.Equip", "Parent Tag for Equip Abilities");
	GameplayTags.AddTag(GameplayTags.Ability_Weapon_Equip_Rifle, "Ability.Weapon.Equip.Rifle", "Equip Rifle Ability");
	GameplayTags.AddTag(GameplayTags.Ability_Weapon_Equip_Pistol, "Ability.Weapon.Equip.Pistol", "Equip Pistol Ability");

	GameplayTags.AddTag(GameplayTags.Ability_Weapon_Fire, "Ability.Weapon.Fire", "Fire Weapon Ability");

	GameplayTags.AddTag(GameplayTags.State_Combat_Rifle, "State.Combat.Rifle", "Character is holding a Rifle");
	GameplayTags.AddTag(GameplayTags.State_Combat_Pistol, "State.Combat.Pistol", "Character is holding a Pistol");
}

void FTimeThiefGameplayTags::AddTag(FGameplayTag& OutTag, const ANSICHAR* TagName, const ANSICHAR* TagComment) {
	OutTag = UGameplayTagsManager::Get().AddNativeGameplayTag(FName(TagName), FString(ANSI_TO_TCHAR(TagComment)));
}