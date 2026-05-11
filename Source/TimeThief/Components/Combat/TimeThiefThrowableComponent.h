#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ItemCommons.h"
#include "Weapon/TimeThiefThrowableTypes.h"
#include "TimeThiefThrowableComponent.generated.h"

class ATimeThiefPlayerCharacter;
class ATimeThiefThrowableProjectile;
class UInventorySystemComponent;
class UTimeThiefThrowableData;

UCLASS(Blueprintable, ClassGroup = (TimeThief), meta = (BlueprintSpawnableComponent))
class TIMETHIEF_API UTimeThiefThrowableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTimeThiefThrowableComponent();

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Throwable")
	void HandleInputPressed(FGameplayTag InputTag);

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Throwable")
	bool TryThrowEquippedThrowable();

	UFUNCTION(BlueprintPure, Category = "TimeThief|Throwable")
	bool CanThrowEquippedThrowable() const;

protected:
	bool CanThrowItem(EItemID ItemID) const;
	bool IsSupportedThrowableItem(EItemID ItemID) const;
	EItemID GetEquippedThrowableItem() const;
	FTimeThiefThrowableDefinition ResolveThrowableDefinition(EItemID ItemID) const;
	FVector ResolveThrowOrigin(const ATimeThiefPlayerCharacter* PlayerCharacter) const;
	FVector ResolveThrowVelocity(const ATimeThiefPlayerCharacter* PlayerCharacter, const FVector& ThrowOrigin, const FTimeThiefThrowableThrowSettings& ThrowSettings) const;
	uint32 ResolveGrenadeTypeForFutureNetwork(EItemID ItemID) const;
	void PlayThrowAnimation(ATimeThiefPlayerCharacter* PlayerCharacter, const FTimeThiefThrowableThrowSettings& ThrowSettings) const;
	void PlayThrowSound(const FTimeThiefThrowableThrowSettings& ThrowSettings, const FVector& ThrowOrigin) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Throwable")
	TSubclassOf<ATimeThiefThrowableProjectile> ThrowableProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Throwable")
	TObjectPtr<UTimeThiefThrowableData> ThrowableData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Throwable")
	FName ThrowSocketName = TEXT("HandGrip_R");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Throwable|Aim", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float AimTraceRange = 50000.0f;

private:
	ATimeThiefPlayerCharacter* GetPlayerCharacter() const;
	UInventorySystemComponent* GetInventoryComponent() const;

	float NextAllowedThrowTime = 0.0f;
};
