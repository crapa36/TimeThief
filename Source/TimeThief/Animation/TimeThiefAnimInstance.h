#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "TimeThiefAnimInstance.generated.h"

class UCharacterTrajectoryComponent;
class IMovableNetworkEntityInterface;
class ACharacter;
class UCharacterMovementComponent;

UCLASS(Config = Game)
class TIMETHIEF_API UTimeThiefAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UTimeThiefAnimInstance(const FObjectInitializer& ObjectInitializer);

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "Jump")
	void TriggerDoubleJump();
	
	UFUNCTION(BlueprintCallable, Category = "Jump", meta=(BlueprintThreadSafe))
	void OnEnterDoubleJumpStart(const FAnimUpdateContext& Context, const FAnimNodeReference& Node);
	
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "References")
	TObjectPtr<ACharacter> CharacterOwner;
	
	TScriptInterface<IMovableNetworkEntityInterface> MovableNetworkInterface;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "References")
	TObjectPtr<UCharacterMovementComponent> CharacterMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "References")
	TObjectPtr<UCharacterTrajectoryComponent> TrajectoryComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Locomotion")
	FVector Velocity;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Locomotion")
	float GroundSpeed;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Locomotion")
	float Direction;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Locomotion")
	bool bHasVelocity;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Locomotion")
	bool bIsMoving;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Locomotion")
	bool bShouldMove;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Locomotion")
	int TurnDirection;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Jump")
	bool bIsFalling;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Jump")
	float VerticalVelocity;

	UPROPERTY(BlueprintReadOnly, Category = "Jump")
	bool bIsDoubleJumping = false;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	FTransform WeaponSocket;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	FVector MeshAlpha;
};