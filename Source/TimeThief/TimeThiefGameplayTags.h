#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class UGameplayTagsManager;

struct FTimeThiefGameplayTags {
public:
	static const FTimeThiefGameplayTags& Get() { return GameplayTags; }
	static void InitializeNativeGameplayTags();

	FGameplayTag InputTag_Action_Move;
	FGameplayTag InputTag_Action_Look;
	FGameplayTag InputTag_Action_Jump;

	FGameplayTag Weapon_Rifle;
	FGameplayTag Weapon_Pistol;

	FGameplayTag Ability_Weapon_Equip;
	FGameplayTag Ability_Weapon_Equip_Rifle;
	FGameplayTag Ability_Weapon_Equip_Pistol;

	FGameplayTag Ability_Weapon_Fire;

	FGameplayTag State_Combat_Rifle;
	FGameplayTag State_Combat_Pistol;

protected:
	void AddTag(FGameplayTag& OutTag, const ANSICHAR* TagName, const ANSICHAR* TagComment);

private:
	static FTimeThiefGameplayTags GameplayTags;
};