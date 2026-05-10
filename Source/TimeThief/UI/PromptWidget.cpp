// Fill out your copyright notice in the Description page of Project Settings.


#include "PromptWidget.h"

#include "Components/TextBlock.h"


void UPromptWidget::SetPromptText(const FText& NewText)
{
	Prompt_Text->SetText(NewText);
}
