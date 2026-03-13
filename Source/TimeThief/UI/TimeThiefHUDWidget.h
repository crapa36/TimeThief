#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TimeThiefHUDWidget.generated.h"

class ATimeThiefPlayerCharacter;
class UTimeThiefHealthComponent;
class UTimeThiefPlayerCombatComponent;

UCLASS(Abstract)
class TIMETHIEF_API UTimeThiefHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "TimeThief|HUD")
	void InitializeHUD(ATimeThiefPlayerCharacter* InCharacter);

protected:
	virtual void NativeConstruct() override;

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|HUD")
	TWeakObjectPtr<ATimeThiefPlayerCharacter> CachedCharacter;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|HUD")
	TWeakObjectPtr<UTimeThiefHealthComponent> CachedHealthComponent;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|HUD")
	TWeakObjectPtr<UTimeThiefPlayerCombatComponent> CachedCombatComponent;

	UFUNCTION()
	void HandleHealthChanged(UTimeThiefHealthComponent* HealthComp, float OldHealth, float NewHealth, AActor* Instigator);

	UFUNCTION(BlueprintImplementableEvent, Category = "TimeThief|HUD|Health")
	void OnHealthUpdated(float CurrentHealth, float MaxHealth, float HealthPercentage);

	UFUNCTION(BlueprintImplementableEvent, Category = "TimeThief|HUD|Ammo")
	void OnAmmoUpdated(int32 CurrentAmmo, int32 MaxAmmo, bool bHasWeapon);

	UFUNCTION(BlueprintImplementableEvent, Category = "TimeThief|HUD|Crosshair")
	void OnCrosshairSpreadUpdated(float SpreadMultiplier, bool bIsAiming);

private:
	void UpdateAmmoDisplay();
	void UpdateCrosshairDisplay();
};