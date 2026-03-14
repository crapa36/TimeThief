#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TimeThiefHUDWidget.generated.h"

class UProgressBar;
class UHorizontalBox;
class UTextBlock;
class UTimePointSystemComponent;
class ATimeThiefPlayerCharacter;
class UTimeThiefHealthComponent;
class UTimeThiefPlayerCombatComponent;

UCLASS(Abstract)
class TIMETHIEF_API UTimeThiefHUDWidget : public UUserWidget
{
	GENERATED_BODY()
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> Health_ProgressBar;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> CurrentHealth_Text;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MaxHealth_Text;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> CurrentAmmo_Text;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MaxAmmo_Text;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> AmmoText_Bar;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TimePoint_Text;
	
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

	UPROPERTY()
	TWeakObjectPtr<UTimePointSystemComponent> CachedTimePointSystemComponent;
	
	void OnHealthUpdated(const UTimeThiefHealthComponent*, float, float, AActor*);
	
	void OnAmmoUpdated(int32 CurrentAmmo, int32 MaxAmmo, bool bHasWeapon);

	UFUNCTION(BlueprintImplementableEvent, Category = "TimeThief|HUD|Crosshair")
	void OnCrosshairSpreadUpdated(float SpreadMultiplier, bool bIsAiming);

	UFUNCTION()
	void OnTimePointUpdated(int DisplayTimePoints);
	
private:
	void UpdateAmmoDisplay();
	void UpdateCrosshairDisplay();
};