#include "TimeThiefGameplayTags.h"
#include "GameplayTagsManager.h"

FTimeThiefGameplayTags FTimeThiefGameplayTags::GameplayTags;

void FTimeThiefGameplayTags::InitializeNativeGameplayTags() {
	GameplayTags.AddAllTags(UGameplayTagsManager::Get());
}

void FTimeThiefGameplayTags::AddAllTags(UGameplayTagsManager& Manager) {
	// 태그 등록: "Input.Action.Move" 등
	InputTag_Action_Move = Manager.AddNativeGameplayTag(FName("Input.Action.Move"), FString("Move input."));
	InputTag_Action_Look = Manager.AddNativeGameplayTag(FName("Input.Action.Look"), FString("Look input."));
	InputTag_Action_Jump = Manager.AddNativeGameplayTag(FName("Input.Action.Jump"), FString("Jump input."));
}