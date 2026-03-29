// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PromptWidget.generated.h"

class UTextBlock;
/**
 * 
 */
UCLASS()
class TIMETHIEF_API UPromptWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	UPROPERTY(meta =(BindWidget))
	TObjectPtr<UTextBlock> ActionKey_Text;

	UPROPERTY(meta =(BindWidget))
	TObjectPtr<UTextBlock> Prompt_Text;
};
