#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "TimeThiefInputConfig.generated.h"

class UInputAction;

// 구조체: 입력 액션과 게임플레이 태그를 묶는 단위
USTRUCT(BlueprintType)
struct FTimeThiefInputAction {
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	const UInputAction* InputAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Meta = (Categories = "Input"))
	FGameplayTag InputTag;
};

/**
 * 입력 액션(IA)과 게임플레이 태그를 매핑하는 설정 파일
 */
UCLASS()
class TIMETHIEF_API UTimeThiefInputConfig : public UDataAsset {
	GENERATED_BODY()

public:
	// 태그로 네이티브 액션(이동, 시점 등) 찾기
	const UInputAction* FindNativeInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound = true) const;

	// 태그로 어빌리티 액션(스킬, 발사 등) 찾기
	const UInputAction* FindAbilityInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound = true) const;

public:
	// C++ 함수에 직접 연결할 액션들 (Move, Look, Jump 등)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Meta = (TitleProperty = "InputAction"))
	TArray<FTimeThiefInputAction> NativeInputActions;

	// GAS 어빌리티 발동용 액션들 (Fire, Skill_Q, Skill_E 등)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Meta = (TitleProperty = "InputAction"))
	TArray<FTimeThiefInputAction> AbilityInputActions;
};