#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class UGameplayTagsManager;

struct FTimeThiefGameplayTags {
public:
	static const FTimeThiefGameplayTags& Get() { return GameplayTags; }
	static void InitializeNativeGameplayTags();

public:
	// 입력 태그 변수 선언
	FGameplayTag InputTag_Action_Move;
	FGameplayTag InputTag_Action_Look;
	FGameplayTag InputTag_Action_Jump;

protected:
	void AddAllTags(UGameplayTagsManager& Manager);

private:
	static FTimeThiefGameplayTags GameplayTags;
};