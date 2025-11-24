#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"
#include "TimeThiefInputConfig.h"
#include "TimeThiefInputComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class TIMETHIEF_API UTimeThiefInputComponent : public UEnhancedInputComponent {
	GENERATED_BODY()

public:
	// 1. 네이티브 액션 바인딩 (이동, 점프, 마우스 회전 등)
	template<class UserClass, typename FuncType>
	void BindNativeAction(const UTimeThiefInputConfig* InputConfig, const FGameplayTag& InputTag, ETriggerEvent TriggerEvent, UserClass* Object, FuncType Func);

	// 2. 어빌리티 액션 바인딩 (스킬, 공격 등 - 태그를 함께 넘김)
	template<class UserClass, typename PressedFuncType, typename ReleasedFuncType>
	void BindAbilityActions(const UTimeThiefInputConfig* InputConfig, UserClass* Object, PressedFuncType PressedFunc, ReleasedFuncType ReleasedFunc, TArray<uint32>& BindHandles);
};

// 템플릿 함수 구현

template<class UserClass, typename FuncType>
void UTimeThiefInputComponent::BindNativeAction(const UTimeThiefInputConfig* InputConfig, const FGameplayTag& InputTag, ETriggerEvent TriggerEvent, UserClass* Object, FuncType Func) {
	check(InputConfig);
	// Config에서 태그에 맞는 IA를 찾아서 바인딩
	if (const UInputAction* IA = InputConfig->FindNativeInputActionForTag(InputTag)) {
		BindAction(IA, TriggerEvent, Object, Func);
	}
}

template<class UserClass, typename PressedFuncType, typename ReleasedFuncType>
void UTimeThiefInputComponent::BindAbilityActions(const UTimeThiefInputConfig* InputConfig, UserClass* Object, PressedFuncType PressedFunc, ReleasedFuncType ReleasedFunc, TArray<uint32>& BindHandles) {
	check(InputConfig);

	for (const FTimeThiefInputAction& Action : InputConfig->AbilityInputActions) {
		if (Action.InputAction && Action.InputTag.IsValid()) {
			// 눌렀을 때 (Pressed) -> PressedFunc 실행 (태그 전달)
			if (PressedFunc) {
				BindHandles.Add(BindAction(Action.InputAction, ETriggerEvent::Started, Object, PressedFunc, Action.InputTag).GetHandle());
			}

			// 뗐을 때 (Released) -> ReleasedFunc 실행 (태그 전달)
			if (ReleasedFunc) {
				BindHandles.Add(BindAction(Action.InputAction, ETriggerEvent::Completed, Object, ReleasedFunc, Action.InputTag).GetHandle());
			}
		}
	}
}