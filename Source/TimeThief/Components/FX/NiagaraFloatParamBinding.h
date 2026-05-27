#pragma once
#include "NiagaraFloatParamBinding.generated.h"


USTRUCT(BlueprintType)
struct FNiagaraFloatParamBinding
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName ParameterName;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Value = 0.0f;
};