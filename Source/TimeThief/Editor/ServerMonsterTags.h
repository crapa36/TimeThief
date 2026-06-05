#pragma once

#include "CoreMinimal.h"

namespace ServerMonsterTags
{
	inline const FName Cat(TEXT("Cat"));
	inline const FName Minion(TEXT("Minion"));
	inline const FName Kong(TEXT("Kong"));

	inline const TArray<FName>& GetAll()
	{
		static const TArray<FName> Tags =
		{
			Cat,
			Minion,
			Kong
		};

		return Tags;
	}

	inline FString GetTypeName(const FName& MonsterTag)
	{
		return MonsterTag.ToString().ToLower();
	}
}
