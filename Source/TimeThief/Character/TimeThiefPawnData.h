#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "TimeThiefPawnData.generated.h"

class UTimeThiefInputConfig;
class UInputMappingContext;

UCLASS(BlueprintType, Const, Meta = (DisplayName = "TimeThief Pawn Data", ShortTooltip = "Data asset used to define a Pawn."))
class TIMETHIEF_API UTimeThiefPawnData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UTimeThiefPawnData(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Pawn")
	TSubclassOf<APawn> PawnClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Input")
	TObjectPtr<UTimeThiefInputConfig> InputConfig;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Input")
	TArray<TObjectPtr<UInputMappingContext>> InputMappingContexts;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Tags", Meta = (Categories = "Pawn"))
	FGameplayTagContainer PawnTags;
};
