#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Weapon/TimeThiefThrowableTypes.h"
#include "TimeThiefThrowableData.generated.h"

UCLASS(BlueprintType)
class TIMETHIEF_API UTimeThiefThrowableData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UTimeThiefThrowableData();

	static FTimeThiefThrowableDefinition MakeDefaultDefinition(EItemID ItemID);
	const FTimeThiefThrowableDefinition* FindDefinition(EItemID ItemID) const;
	FTimeThiefThrowableDefinition GetDefinitionOrDefault(EItemID ItemID) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Throwable")
	TMap<EItemID, FTimeThiefThrowableDefinition> ThrowableDefinitions;
};
