#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TimeThiefHUDWidget.generated.h"

class UProgressBar;
class UHorizontalBox;
class UTextBlock;
class UImage;
class UTimePointSystemComponent;
class ATimeThiefPlayerCharacter;
class UTimeThiefHealthComponent;
class UTimeThiefPlayerCombatComponent;
class UTimeThiefWeaponComponentBase;
class UTimeThiefWireComponent;
class UTimeThiefControlGuideWidget;

UCLASS(Abstract)
class TIMETHIEF_API UTimeThiefHUDWidget : public UUserWidget
{
	GENERATED_BODY()
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> Health_ProgressBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> WireCooldown_ProgressBar;
	
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

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Crosshair_Image;
	
public:
	UTimeThiefHUDWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "TimeThief|HUD")
	void InitializeHUD(ATimeThiefPlayerCharacter* InCharacter);

	UFUNCTION(BlueprintCallable, Category = "TimeThief|HUD")
	void ToggleControlGuideWidget();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(EditDefaultsOnly, Category = "TimeThief|HUD")
	TSubclassOf<UTimeThiefControlGuideWidget> ControlGuideWidgetClass;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|HUD")
	TWeakObjectPtr<ATimeThiefPlayerCharacter> CachedCharacter;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|HUD")
	TWeakObjectPtr<UTimeThiefHealthComponent> CachedHealthComponent;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|HUD")
	TWeakObjectPtr<UTimeThiefPlayerCombatComponent> CachedCombatComponent;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|HUD")
	TWeakObjectPtr<UTimeThiefWireComponent> CachedWireComponent;

	UPROPERTY()
	TWeakObjectPtr<UTimePointSystemComponent> CachedTimePointSystemComponent;
	
	void OnHealthUpdated(const UTimeThiefHealthComponent*, float, float, AActor*);
	void OnAmmoUpdated(int32 CurrentAmmo, int32 MaxAmmo, bool bHasWeapon);
	
	void HandleAmmoChanged(int32 CurrentAmmo, int32 MaxAmmo);
	void OnWeaponEquipped(UTimeThiefWeaponComponentBase* Weapon);
	void OnWeaponUnequipped();

	UFUNCTION()
	void OnTimePointUpdated(int DisplayTimePoints);
	
private:
	void EnsureControlGuideWidget();
	void UpdateCrosshairDisplay();
	void UpdateWireCooldownDisplay();
	
	TWeakObjectPtr<UTimeThiefWeaponComponentBase> CachedWeapon;

	UPROPERTY(Transient)
	TObjectPtr<UTimeThiefControlGuideWidget> ControlGuideWidget;
	float LastCrosshairScale = -1.0f;
	float LastWireCooldownPercent = -1.0f;
};
