#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TimeThiefHUDWidget.generated.h"

class UProgressBar;
class UHorizontalBox;
class UImage;
class UTextBlock;
class UTexture2D;
class UTimePointSystemComponent;
class ATimeThiefPlayerCharacter;
class UTimeThiefHealthComponent;
class UTimeThiefPlayerCombatComponent;
class UTimeThiefSkillComponent;
class UTimeThiefWeaponComponentBase;
class UTimeThiefWireComponent;
class USavePointSkillComponent;

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

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> SkillSlot1_Icon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> SkillSlot1_Cooldown_ProgressBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SkillSlot1_Cooldown_Text;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> SkillSlot2_Icon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> SkillSlot2_Cooldown_ProgressBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SkillSlot2_Cooldown_Text;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> SavePoint_Icon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> SavePoint_Cooldown_ProgressBar;
	
public:
	UTimeThiefHUDWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "TimeThief|HUD")
	void InitializeHUD(ATimeThiefPlayerCharacter* InCharacter);

	UFUNCTION(BlueprintCallable, Category = "TimeThief|HUD")
	void ToggleControlGuideWidget();

	UFUNCTION(BlueprintCallable, Category = "TimeThief|HUD")
	bool HideControlGuideWidget();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	                          const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	                          int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	UPROPERTY(EditDefaultsOnly, Category = "TimeThief|HUD")
	TSubclassOf<UUserWidget> ControlGuideWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "TimeThief|HUD|Crosshair")
	FLinearColor CrosshairLineColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.85f);

	UPROPERTY(EditDefaultsOnly, Category = "TimeThief|HUD|Crosshair", meta = (ClampMin = "0.0"))
	float CrosshairBaseGap = 12.0f;

	UPROPERTY(EditDefaultsOnly, Category = "TimeThief|HUD|Crosshair", meta = (ClampMin = "0.0"))
	float CrosshairSpreadGapScale = 8.0f;

	UPROPERTY(EditDefaultsOnly, Category = "TimeThief|HUD|Crosshair", meta = (ClampMin = "0.0"))
	float CrosshairMaxSpreadGap = 48.0f;

	UPROPERTY(EditDefaultsOnly, Category = "TimeThief|HUD|Crosshair", meta = (ClampMin = "0.0"))
	float CrosshairLineLength = 30.0f;

	UPROPERTY(EditDefaultsOnly, Category = "TimeThief|HUD|Crosshair", meta = (ClampMin = "0.0"))
	float CrosshairLineThickness = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "TimeThief|HUD|Crosshair", meta = (ClampMin = "0.0"))
	float CrosshairCenterDotRadius = 3.0f;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|HUD")
	TWeakObjectPtr<ATimeThiefPlayerCharacter> CachedCharacter;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|HUD")
	TWeakObjectPtr<UTimeThiefHealthComponent> CachedHealthComponent;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|HUD")
	TWeakObjectPtr<UTimeThiefPlayerCombatComponent> CachedCombatComponent;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|HUD")
	TWeakObjectPtr<UTimeThiefWireComponent> CachedWireComponent;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|HUD")
	TWeakObjectPtr<UTimeThiefSkillComponent> CachedSkillComponent;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|HUD")
	TWeakObjectPtr<USavePointSkillComponent> CachedSavePointSkillComponent;

	UPROPERTY()
	TWeakObjectPtr<UTimePointSystemComponent> CachedTimePointSystemComponent;
	
	void OnHealthUpdated(const UTimeThiefHealthComponent*, float, float, AActor*);
	void OnAmmoUpdated(int32 CurrentAmmo, int32 MaxAmmo, bool bHasWeapon);
	
	void HandleAmmoChanged(int32 CurrentAmmo, int32 MaxAmmo);
	void OnWeaponEquipped(UTimeThiefWeaponComponentBase* Weapon);
	void OnWeaponUnequipped();
	void OnSkillSlotsChanged();
	void OnSkillCooldownChanged(uint32 SlotIndex);

	UFUNCTION()
	void OnTimePointUpdated(int DisplayTimePoints);
	
private:
	void EnsureControlGuideWidget();
	UUserWidget* GetControlGuideWidget() const;
	void RefreshControlGuideWidget() const;
	void UpdateCrosshairInvalidation();
	void UpdateWireCooldownDisplay();
	void UpdateSkillSlotsDisplay();
	void UpdateSkillSlotDisplay(uint32 SlotIndex, UImage* IconWidget, UProgressBar* ProgressBarWidget);
	void UpdateSkillCooldownDisplay();
	void UpdateSkillCooldownSlotDisplay(uint32 SlotIndex, UProgressBar* CooldownProgressBar, UTextBlock* CooldownText, float& LastCooldownPercent, int32& LastCooldownSeconds);
	void UpdateSavePointCooldownDisplay();
	UTexture2D* ResolveSkillIcon(int32 SkillId) const;
	
	TWeakObjectPtr<UTimeThiefWeaponComponentBase> CachedWeapon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UUserWidget> ControlGuideWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> SpawnedControlGuideWidget;
	float LastCrosshairSpread = -1.0f;
	float LastWireCooldownPercent = -1.0f;
	float LastSkillSlot1CooldownPercent = -1.0f;
	float LastSkillSlot2CooldownPercent = -1.0f;
	float LastSavePointCooldownPercent = -1.0f;
	float LastSavePointIconOpacity = -1.0f;
	int32 LastSkillSlot1CooldownSeconds = -1;
	int32 LastSkillSlot2CooldownSeconds = -1;
};
