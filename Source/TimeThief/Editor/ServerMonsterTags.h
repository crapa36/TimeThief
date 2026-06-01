#pragma once

#include "CoreMinimal.h"

namespace ServerMonsterTags
{
	inline const FName Cat(TEXT("Cat"));

	inline const TArray<FName>& GetAll()
	{
		static const TArray<FName> Tags =
		{
			Cat,
		};

		return Tags;
	}

	inline FString GetTypeName(const FName& MonsterTag)
	{
		return MonsterTag.ToString().ToLower();
	}
}
