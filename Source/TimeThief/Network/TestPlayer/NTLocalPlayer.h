#pragma once

#include "CoreMinimal.h"
#include "InputActionValue.h"
#include "NTPlayer.h"
#include "Developer/Datasmith/DatasmithFacade/Public/DatasmithFacadeActor.h"
#include "NTLocalPlayer.generated.h"

class UInputAction;
class UInputMappingContext;

UCLASS()
class TIMETHIEF_API ANTLocalPlayer : public ANTPlayer
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ANTLocalPlayer();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
protected:
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	
public:
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoMove(float Right, float Forward);
	
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoLook(float Yaw, float Pitch);
	
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoJumpStart();
	
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoJumpEnd();
	
public:
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	FORCEINLINE class UCameraComponent* GetCamera() const { return FollowCamera; }
	
private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;
	
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FollowCamera;
	
protected:
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputMappingContext> IMC_Default;
	
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;
	
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> LookAction;
	
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> MouseLookAction;
	
protected:
	// TEMP
	const float MOVE_PACKET_SEND_DELAY = 0.1f; // 100ms마다 MoveInput 패킷 전송
	
	FVector2D DesiredInput = FVector2D::ZeroVector;
	FVector DesiredMoveDirection = FVector::ZeroVector;
	float DesiredYaw;
	
	FVector2D LastDesiredInput = FVector2D::ZeroVector;
	
	float MovePacketElapsed = 0.0f;
	
};
