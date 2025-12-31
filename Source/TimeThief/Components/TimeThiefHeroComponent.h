#pragma once

#include "CoreMinimal.h"
#include "Components/TimeThiefPawnExtensionComponent.h"
#include "GameplayTagContainer.h"
#include "InputActionValue.h"
#include "TimeThiefHeroComponent.generated.h"

class UTimeThiefPawnData;
class UTimeThiefInputConfig;
class UTimeThiefCameraMode;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTimeThiefHero_ReadyDelegate, UTimeThiefHeroComponent*, HeroComponent);

UCLASS(Blueprintable, Meta = (BlueprintSpawnableComponent))
class TIMETHIEF_API UTimeThiefHeroComponent : public UTimeThiefPawnExtensionComponent {
	GENERATED_BODY()

public:
	UTimeThiefHeroComponent(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintPure, Category = "TimeThief|Hero")
	static UTimeThiefHeroComponent* FindHeroComponent(const AActor* Actor);

	void SetPawnData(const UTimeThiefPawnData* InPawnData);

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Hero")
	bool IsReadyToBindInputs() const { return bReadyToBindInputs; }

	UPROPERTY(BlueprintAssignable)
	FTimeThiefHero_ReadyDelegate OnReadyToBindInputs;

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Hero")
	void AddAdditionalInputConfig(const UTimeThiefInputConfig* InputConfig);

	void InitializePlayerInput(UInputComponent* PlayerInputComponent);

protected:
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void Input_AbilityInputTagPressed(FGameplayTag InputTag);
	void Input_AbilityInputTagReleased(FGameplayTag InputTag);

	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	void Input_Jump(const FInputActionValue& Value);

protected:
	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|Hero")
	TObjectPtr<const UTimeThiefPawnData> PawnData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Hero")
	float RotationInterpSpeed = 10.0f;

	bool bReadyToBindInputs = false;
};

