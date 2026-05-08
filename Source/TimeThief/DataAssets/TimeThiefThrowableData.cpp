#include "DataAssets/TimeThiefThrowableData.h"

#include "Actors/TimeThiefSmokeDebugVolume.h"

FTimeThiefThrowableDefinition UTimeThiefThrowableData::MakeDefaultDefinition(EItemID ItemID)
{
	FTimeThiefThrowableDefinition Definition;
	Definition.ProjectileSettings.SmokeDebugVolumeClass = ATimeThiefSmokeDebugVolume::StaticClass();

	switch (ItemID)
	{
	case EItemID::Grenade:
		Definition.ProjectileSettings.bApplyRadialDamage = true;
		Definition.ProjectileSettings.bSpawnSmokeDebugVolume = false;
		Definition.ProjectileSettings.bDrawDamageDebug = true;
		break;
	case EItemID::SmokeGrenade:
		Definition.ProjectileSettings.bApplyRadialDamage = false;
		Definition.ProjectileSettings.bSpawnSmokeDebugVolume = true;
		Definition.ProjectileSettings.bDrawDamageDebug = false;
		break;
	default:
		Definition.ProjectileSettings.bDrawDamageDebug = false;
		break;
	}

	return Definition;
}

UTimeThiefThrowableData::UTimeThiefThrowableData()
{
	ThrowableDefinitions.Add(EItemID::Grenade, MakeDefaultDefinition(EItemID::Grenade));
	ThrowableDefinitions.Add(EItemID::SmokeGrenade, MakeDefaultDefinition(EItemID::SmokeGrenade));
}

const FTimeThiefThrowableDefinition* UTimeThiefThrowableData::FindDefinition(EItemID ItemID) const
{
	return ThrowableDefinitions.Find(ItemID);
}

FTimeThiefThrowableDefinition UTimeThiefThrowableData::GetDefinitionOrDefault(EItemID ItemID) const
{
	if (const FTimeThiefThrowableDefinition* FoundDefinition = FindDefinition(ItemID))
	{
		return *FoundDefinition;
	}

	return MakeDefaultDefinition(ItemID);
}
