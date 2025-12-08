#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "TimeThiefAssetManager.generated.h"

UCLASS()
class TIMETHIEF_API UTimeThiefAssetManager : public UAssetManager {
	GENERATED_BODY()

public:
	static UTimeThiefAssetManager& Get();

	virtual void StartInitialLoading() override;
};