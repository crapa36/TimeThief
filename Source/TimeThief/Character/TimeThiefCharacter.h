// TimeThiefCharacter.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h" // 태그 헤더 추가
#include "TimeThiefCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UTimeThiefInputConfig; // 전방 선언 변경
struct FInputActionValue;

UCLASS(config = Game)
class ATimeThiefCharacter : public ACharacter {
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

public:
	ATimeThiefCharacter();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UTimeThiefInputConfig> InputConfig;

protected:
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** 네이티브 액션 핸들러 */
	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);

	/** GAS 어빌리티 입력 핸들러 */
	void Input_AbilityInputTagPressed(FGameplayTag InputTag);
	void Input_AbilityInputTagReleased(FGameplayTag InputTag);

public:
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};