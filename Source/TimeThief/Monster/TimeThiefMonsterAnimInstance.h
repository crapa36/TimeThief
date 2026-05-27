

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"

#include "TimeThiefMonsterAnimInstance.generated.h"

class ATimeThiefMonster;
/**
 * 
 */
UCLASS()
class TIMETHIEF_API UTimeThiefMonsterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	UPROPERTY(BlueprintReadOnly, Category="Owner")
	TObjectPtr<ATimeThiefMonster> OwnerMonster = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Movement")
	float Speed = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Aim Offset")
	float AimYaw = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Aim Offset")
	float AimPitch = 0.0f;
};
