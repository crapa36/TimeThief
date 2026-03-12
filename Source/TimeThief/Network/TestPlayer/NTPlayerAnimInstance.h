

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"

#include "NTPlayerAnimInstance.generated.h"

class ANTPlayer;

/**
 * 
 */
UCLASS()
class TIMETHIEF_API UNTPlayerAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
	
public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	
protected:
	UPROPERTY(BlueprintReadWrite, Category="Player")
	TObjectPtr<ANTPlayer> OwnerPlayer = nullptr;
	
	UPROPERTY(BlueprintReadWrite, Category="Movement")
	float Speed = 0.f;
	
	UPROPERTY(BlueprintReadWrite, Category="Movement")
	bool IsAir = false;
	
	UPROPERTY(BlueprintReadWrite, Category="Movement")
	FVector Velocity = FVector::ZeroVector;
	
	UPROPERTY(BlueprintReadWrite, Category="Movement")
	float Direction = 0.f;
	
};
