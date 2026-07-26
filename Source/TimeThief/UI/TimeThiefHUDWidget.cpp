#include "UI/TimeThiefHUDWidget.h"
#include "Character/TimeThiefPlayerCharacter.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Skill/SavePointSkillComponent.h"
#include "Components/Skill/TimeThiefSkillComponent.h"
#include "Components/TimeThiefHealthComponent.h"
#include "Components/Combat/TimeThiefPlayerCombatComponent.h"
#include "Components/System/TimePointSystemComponent.h"
#include "Components/Wire/TimeThiefWireComponent.h"
#include "DataAssets/GameItemData.h"
#include "Game/ItemSettings.h"
#include "ItemCommons.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "UI/TimeThiefControlGuideWidget.h"
#include "Components/Border.h"
#include "Weapon/Components/TimeThiefWeaponComponentBase.h"

namespace
{
	constexpr uint32 SkillSlot1Index = 0;
	constexpr uint32 SkillSlot2Index = 1;

	EItemID ResolveSkillItemId(int32 SkillId)
	{
		switch (SkillId)
		{
		case 1:
			return EItemID::Skill1;
		case 2:
			return EItemID::Skill2;
		case 3:
			return EItemID::Skill3;
		default:
			return EItemID::SIZE;
		}
	}
}

UTimeThiefHUDWidget::UTimeThiefHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UTimeThiefHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	LastCrosshairSpread = -1.0f;

	if (WireCooldown_ProgressBar)
	{
		WireCooldown_ProgressBar->SetVisibility(ESlateVisibility::Collapsed);
		WireCooldown_ProgressBar->SetPercent(0.0f);
	}
	LastWireCooldownPercent = -1.0f;
	LastSkillSlot1CooldownPercent = -1.0f;
	LastSkillSlot2CooldownPercent = -1.0f;
	LastSavePointCooldownPercent = -1.0f;
	LastSavePointIconOpacity = -1.0f;
	LastSkillSlot1CooldownSeconds = -1;
	LastSkillSlot2CooldownSeconds = -1;

	if (SkillSlot1_Icon)
	{
		SkillSlot1_Icon->SetVisibility(ESlateVisibility::Hidden);
	}
	if (SkillSlot2_Icon)
	{
		SkillSlot2_Icon->SetVisibility(ESlateVisibility::Hidden);
	}
	if (SkillSlot1_Cooldown_ProgressBar)
	{
		SkillSlot1_Cooldown_ProgressBar->SetVisibility(ESlateVisibility::Hidden);
		SkillSlot1_Cooldown_ProgressBar->SetPercent(0.0f);
	}
	if (SkillSlot2_Cooldown_ProgressBar)
	{
		SkillSlot2_Cooldown_ProgressBar->SetVisibility(ESlateVisibility::Hidden);
		SkillSlot2_Cooldown_ProgressBar->SetPercent(0.0f);
	}
	if (SkillSlot1_Cooldown_Text)
	{
		SkillSlot1_Cooldown_Text->SetVisibility(ESlateVisibility::Hidden);
	}
	if (SkillSlot2_Cooldown_Text)
	{
		SkillSlot2_Cooldown_Text->SetVisibility(ESlateVisibility::Hidden);
	}
	if (SavePoint_Icon)
	{
		SavePoint_Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
		SavePoint_Icon->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.35f));
	}
	if (SavePoint_Cooldown_ProgressBar)
	{
		SavePoint_Cooldown_ProgressBar->SetVisibility(ESlateVisibility::HitTestInvisible);
		SavePoint_Cooldown_ProgressBar->SetPercent(0.0f);
		SavePoint_Cooldown_ProgressBar->SetFillColorAndOpacity(FLinearColor::White);
	}

	EnsureControlGuideWidget();
	if (UUserWidget* GuideWidget = GetControlGuideWidget())
	{
		GuideWidget->SetVisibility(ESlateVisibility::Hidden);
		RefreshControlGuideWidget();
	}

	if (ATimeThiefPlayerCharacter* PlayerChar = Cast<ATimeThiefPlayerCharacter>(GetOwningPlayerPawn()))
	{
		InitializeHUD(PlayerChar);
	}
}

void UTimeThiefHUDWidget::NativeDestruct()
{
	if (CachedWeapon.IsValid())
	{
		CachedWeapon->OnAmmoChanged_Delegate.RemoveAll(this);
	}

	if (CachedHealthComponent.IsValid())
	{
		CachedHealthComponent->OnHealthChanged_Delegate.RemoveAll(this);
	}

	if (CachedCombatComponent.IsValid())
	{
		CachedCombatComponent->OnWeaponEquipped_Delegate.RemoveAll(this);
		CachedCombatComponent->OnWeaponUnequipped_Delegate.RemoveAll(this);
	}

	if (CachedSkillComponent.IsValid())
	{
		CachedSkillComponent->OnSkillSlotsChanged.RemoveAll(this);
		CachedSkillComponent->OnSkillCooldownChanged.RemoveAll(this);
	}

	if (CachedTimePointSystemComponent.IsValid())
	{
		CachedTimePointSystemComponent->OnTimePointsChanged_Delegate.RemoveAll(this);
	}

	CachedWeapon.Reset();
	CachedWireComponent.Reset();
	CachedSkillComponent.Reset();
	CachedSavePointSkillComponent.Reset();

	if (SpawnedControlGuideWidget)
	{
		SpawnedControlGuideWidget->RemoveFromParent();
		SpawnedControlGuideWidget = nullptr;
	}

	Super::NativeDestruct();
}

void UTimeThiefHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (CachedCharacter.IsValid())
	{
		UpdateCrosshairInvalidation();
		UpdateWireCooldownDisplay();
		UpdateSkillCooldownDisplay();
		UpdateSavePointCooldownDisplay();
	}
}

int32 UTimeThiefHUDWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
                                       const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
                                       int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 MaxLayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId,
	                                            InWidgetStyle, bParentEnabled);

	if (!CachedWeapon.IsValid())
	{
		return MaxLayerId;
	}

	const FVector2D Center = AllottedGeometry.GetLocalSize() * 0.5f;
	const float SpreadGap = FMath::Clamp(
		CachedWeapon->GetCurrentSpread() * CrosshairSpreadGapScale,
		0.0f,
		CrosshairMaxSpreadGap);
	const float Gap = FMath::Max(0.0f, CrosshairBaseGap + SpreadGap);
	const float LineLength = FMath::Max(0.0f, CrosshairLineLength);
	const float LineThickness = FMath::Max(0.0f, CrosshairLineThickness);
	const FLinearColor DrawColor = CrosshairLineColor * InWidgetStyle.GetColorAndOpacityTint();
	const int32 CrosshairLayerId = MaxLayerId + 1;
	const FPaintGeometry PaintGeometry = AllottedGeometry.ToPaintGeometry();

	if (LineLength > 0.0f && LineThickness > 0.0f)
	{
		constexpr float LineAngles[] = { -90.0f, 30.0f, 150.0f };
		for (const float Angle : LineAngles)
		{
			const float Radians = FMath::DegreesToRadians(Angle);
			const FVector2D Direction(FMath::Cos(Radians), FMath::Sin(Radians));

			TArray<FVector2D> LinePoints;
			LinePoints.Reserve(2);
			LinePoints.Add(Center + Direction * Gap);
			LinePoints.Add(Center + Direction * (Gap + LineLength));

			FSlateDrawElement::MakeLines(
				OutDrawElements,
				CrosshairLayerId,
				PaintGeometry,
				MoveTemp(LinePoints),
				ESlateDrawEffect::None,
				DrawColor,
				true,
				LineThickness);
		}
	}

	const float DotRadius = FMath::Max(0.0f, CrosshairCenterDotRadius);
	if (DotRadius > 0.0f)
	{
		const FVector2D DotSize(DotRadius * 2.0f, DotRadius * 2.0f);
		const FVector2D DotPosition = Center - FVector2D(DotRadius, DotRadius);
		const FSlateRoundedBoxBrush DotBrush(FLinearColor::White, DotRadius, DotSize);

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			CrosshairLayerId,
			AllottedGeometry.ToPaintGeometry(DotSize, FSlateLayoutTransform(DotPosition)),
			&DotBrush,
			ESlateDrawEffect::None,
			DrawColor);
	}

	return CrosshairLayerId;
}

void UTimeThiefHUDWidget::InitializeHUD(ATimeThiefPlayerCharacter* InCharacter)
{
	if (!InCharacter) return;

	if (CachedCharacter.IsValid() && CachedCharacter.Get() == InCharacter)
	{
		return;
	}

	if (CachedCharacter.IsValid())
	{
		if (CachedHealthComponent.IsValid())
		{
			CachedHealthComponent->OnHealthChanged_Delegate.RemoveAll(this);
		}
		if (CachedCombatComponent.IsValid())
		{
			CachedCombatComponent->OnWeaponEquipped_Delegate.RemoveAll(this);
			CachedCombatComponent->OnWeaponUnequipped_Delegate.RemoveAll(this);
		}
		if (CachedTimePointSystemComponent.IsValid())
		{
			CachedTimePointSystemComponent->OnTimePointsChanged_Delegate.RemoveAll(this);
		}
		if (CachedSkillComponent.IsValid())
		{
			CachedSkillComponent->OnSkillSlotsChanged.RemoveAll(this);
			CachedSkillComponent->OnSkillCooldownChanged.RemoveAll(this);
		}
		if (CachedWeapon.IsValid())
		{
			CachedWeapon->OnAmmoChanged_Delegate.RemoveAll(this);
		}
	}

	CachedCharacter = InCharacter;
	CachedHealthComponent = InCharacter->GetComponentByClass<UTimeThiefHealthComponent>();
	CachedCombatComponent = InCharacter->GetPlayerCombatComponent();
	CachedWireComponent = InCharacter->GetWireComponent();
	CachedSkillComponent = InCharacter->FindComponentByClass<UTimeThiefSkillComponent>();
	CachedSavePointSkillComponent = InCharacter->GetSavePointSkillComponent();
	CachedTimePointSystemComponent = InCharacter->GetComponentByClass<UTimePointSystemComponent>();
	
	if (CachedHealthComponent.IsValid())
	{
		CachedHealthComponent->OnHealthChanged_Delegate.AddUObject(this, &UTimeThiefHUDWidget::OnHealthUpdated);
		OnHealthUpdated(CachedHealthComponent.Get(), CachedHealthComponent->GetCurrentHealth(), CachedHealthComponent->GetCurrentHealth(), nullptr);
	}
	
	if (CachedTimePointSystemComponent.IsValid())
	{
		CachedTimePointSystemComponent->OnTimePointsChanged_Delegate.AddUObject(this, &UTimeThiefHUDWidget::OnTimePointUpdated);
		OnTimePointUpdated(CachedTimePointSystemComponent->GetTimePoints());
	}

	if (CachedCombatComponent.IsValid())
	{
		CachedCombatComponent->OnWeaponEquipped_Delegate.AddUObject(this, &UTimeThiefHUDWidget::OnWeaponEquipped);
		CachedCombatComponent->OnWeaponUnequipped_Delegate.AddUObject(this, &UTimeThiefHUDWidget::OnWeaponUnequipped);

		if (UTimeThiefWeaponComponentBase* Weapon = CachedCombatComponent->GetCharacterCurrentEquippedWeapon())
		{
			OnWeaponEquipped(Weapon);
		}
		else
		{
			OnWeaponUnequipped();
		}
	}

	if (CachedSkillComponent.IsValid())
	{
		CachedSkillComponent->OnSkillSlotsChanged.AddUObject(this, &UTimeThiefHUDWidget::OnSkillSlotsChanged);
		CachedSkillComponent->OnSkillCooldownChanged.AddUObject(this, &UTimeThiefHUDWidget::OnSkillCooldownChanged);
	}
	UpdateSkillSlotsDisplay();
	UpdateSkillCooldownDisplay();
	UpdateSavePointCooldownDisplay();
}

void UTimeThiefHUDWidget::OnHealthUpdated(const UTimeThiefHealthComponent* HealthComponent, float OldHealth, float CurrHealth, AActor* Instigator)
{
	Health_ProgressBar->SetPercent(HealthComponent->GetHealthPercent());
	
	CurrentHealth_Text->SetText(FText::AsNumber(static_cast<int>(CurrHealth)));
	MaxHealth_Text->SetText(FText::AsNumber(static_cast<int>(HealthComponent->GetMaxHealth())));
}

void UTimeThiefHUDWidget::OnAmmoUpdated(int32 CurrentAmmo, int32 MaxAmmo, bool bHasWeapon)
{
	if (bHasWeapon)
	{
		AmmoText_Bar->SetVisibility(ESlateVisibility::Visible);
		CurrentAmmo_Text->SetText(FText::AsNumber(CurrentAmmo));
		MaxAmmo_Text->SetText(FText::AsNumber(MaxAmmo));
	}
	else
	{
		AmmoText_Bar->SetVisibility(ESlateVisibility::Hidden);
	}
}

void UTimeThiefHUDWidget::HandleAmmoChanged(int32 CurrentAmmo, int32 MaxAmmo)
{
	OnAmmoUpdated(CurrentAmmo, MaxAmmo, true);
}

void UTimeThiefHUDWidget::OnWeaponEquipped(UTimeThiefWeaponComponentBase* Weapon)
{
	if (CachedWeapon.IsValid() && CachedWeapon.Get() != Weapon)
	{
		CachedWeapon->OnAmmoChanged_Delegate.RemoveAll(this);
	}

	CachedWeapon = Weapon;

	LastCrosshairSpread = -1.0f;
	Invalidate(EInvalidateWidgetReason::Paint);

	if (Weapon)
	{
		Weapon->OnAmmoChanged_Delegate.AddUObject(this, &UTimeThiefHUDWidget::HandleAmmoChanged);
		HandleAmmoChanged(Weapon->GetCurrentAmmo(), Weapon->GetMaxAmmo());
	}
	else
	{
		OnAmmoUpdated(0, 0, false);
	}
}

void UTimeThiefHUDWidget::OnWeaponUnequipped()
{
	if (CachedWeapon.IsValid())
	{
		CachedWeapon->OnAmmoChanged_Delegate.RemoveAll(this);
		CachedWeapon.Reset();
	}

	LastCrosshairSpread = -1.0f;
	Invalidate(EInvalidateWidgetReason::Paint);

	OnAmmoUpdated(0, 0, false);
}

void UTimeThiefHUDWidget::OnTimePointUpdated(int DisplayTimePoints)
{
	TimePoint_Text->SetText(FText::AsNumber(DisplayTimePoints));
}

void UTimeThiefHUDWidget::OnSkillSlotsChanged()
{
	UpdateSkillSlotsDisplay();
	UpdateSkillCooldownDisplay();
}

void UTimeThiefHUDWidget::OnSkillCooldownChanged(uint32 SlotIndex)
{
	if (SlotIndex == SkillSlot1Index)
	{
		UpdateSkillCooldownSlotDisplay(
			SkillSlot1Index,
			SkillSlot1_Cooldown_ProgressBar,
			SkillSlot1_Cooldown_Text,
			LastSkillSlot1CooldownPercent,
			LastSkillSlot1CooldownSeconds);
	}
	else if (SlotIndex == SkillSlot2Index)
	{
		UpdateSkillCooldownSlotDisplay(
			SkillSlot2Index,
			SkillSlot2_Cooldown_ProgressBar,
			SkillSlot2_Cooldown_Text,
			LastSkillSlot2CooldownPercent,
			LastSkillSlot2CooldownSeconds);
	}
}

void UTimeThiefHUDWidget::ToggleControlGuideWidget()
{
	EnsureControlGuideWidget();
	UUserWidget* GuideWidget = GetControlGuideWidget();
	if (!GuideWidget)
	{
		return;
	}

	if (GuideWidget->IsVisible())
	{
		GuideWidget->SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	RefreshControlGuideWidget();
	GuideWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
}

bool UTimeThiefHUDWidget::HideControlGuideWidget()
{
	UUserWidget* GuideWidget = GetControlGuideWidget();
	if (!GuideWidget || !GuideWidget->IsVisible())
	{
		return false;
	}

	GuideWidget->SetVisibility(ESlateVisibility::Hidden);
	return true;
}

void UTimeThiefHUDWidget::EnsureControlGuideWidget()
{
	if (GetControlGuideWidget() || !ControlGuideWidgetClass)
	{
		return;
	}

	SpawnedControlGuideWidget = CreateWidget<UUserWidget>(GetOwningPlayer(), ControlGuideWidgetClass);
	if (!SpawnedControlGuideWidget)
	{
		return;
	}

	SpawnedControlGuideWidget->AddToViewport();
	SpawnedControlGuideWidget->SetVisibility(ESlateVisibility::Hidden);
}

UUserWidget* UTimeThiefHUDWidget::GetControlGuideWidget() const
{
	return ControlGuideWidget ? ControlGuideWidget.Get() : SpawnedControlGuideWidget.Get();
}

void UTimeThiefHUDWidget::RefreshControlGuideWidget() const
{
	if (UTimeThiefControlGuideWidget* TypedControlGuideWidget = Cast<UTimeThiefControlGuideWidget>(GetControlGuideWidget()))
	{
		TypedControlGuideWidget->RefreshControlGuide();
	}
}

void UTimeThiefHUDWidget::UpdateCrosshairInvalidation()
{
	if (!CachedWeapon.IsValid())
	{
		if (!FMath::IsNearlyEqual(LastCrosshairSpread, -1.0f, KINDA_SMALL_NUMBER))
		{
			LastCrosshairSpread = -1.0f;
			Invalidate(EInvalidateWidgetReason::Paint);
		}
		return;
	}

	const float CurrentSpread = CachedWeapon->GetCurrentSpread();
	if (!FMath::IsNearlyEqual(LastCrosshairSpread, CurrentSpread, KINDA_SMALL_NUMBER))
	{
		LastCrosshairSpread = CurrentSpread;
		Invalidate(EInvalidateWidgetReason::Paint);
	}
}

void UTimeThiefHUDWidget::UpdateWireCooldownDisplay()
{
	if (!WireCooldown_ProgressBar)
	{
		return;
	}

	const float CooldownRemaining = CachedWireComponent.IsValid() ? CachedWireComponent->GetCooldownRemaining() : 0.0f;
	if (CooldownRemaining <= 0.0f)
	{
		if (!FMath::IsNearlyZero(LastWireCooldownPercent))
		{
			WireCooldown_ProgressBar->SetPercent(0.0f);
			LastWireCooldownPercent = 0.0f;
		}
		if (WireCooldown_ProgressBar->GetVisibility() != ESlateVisibility::Collapsed)
		{
			WireCooldown_ProgressBar->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	const float CooldownPercent = CachedWireComponent->GetCooldownPercent();
	if (!FMath::IsNearlyEqual(LastWireCooldownPercent, CooldownPercent, KINDA_SMALL_NUMBER))
	{
		WireCooldown_ProgressBar->SetPercent(CooldownPercent);
		LastWireCooldownPercent = CooldownPercent;
	}
	if (WireCooldown_ProgressBar->GetVisibility() != ESlateVisibility::HitTestInvisible)
	{
		WireCooldown_ProgressBar->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UTimeThiefHUDWidget::UpdateSkillSlotsDisplay()
{
	UpdateSkillSlotDisplay(SkillSlot1Index, SkillSlot1_Icon, SkillSlot1_Cooldown_ProgressBar, SkillKey1);
	UpdateSkillSlotDisplay(SkillSlot2Index, SkillSlot2_Icon, SkillSlot2_Cooldown_ProgressBar, SkillKey2);
}

void UTimeThiefHUDWidget::UpdateSkillSlotDisplay(uint32 SlotIndex, UImage* IconWidget, UProgressBar* ProgressBarWidget, UBorder* SkillKey)
{
	FTimeThiefSkillSlotState SlotState;
	const bool bHasSkill = CachedSkillComponent.IsValid()
		&& CachedSkillComponent->GetSkillSlotState(static_cast<int32>(SlotIndex), SlotState)
		&& SlotState.SkillId > 0;

	if (!bHasSkill)
	{
		if (IconWidget)
		{
			IconWidget->SetVisibility(ESlateVisibility::Hidden);
		}
		if (ProgressBarWidget)
		{
			ProgressBarWidget->SetVisibility(ESlateVisibility::Hidden);
		}
		if (SkillKey)
		{
			SkillKey->SetVisibility(ESlateVisibility::Hidden);
		}
		return;
	}

	if (ProgressBarWidget)
	{
		ProgressBarWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	if (!IconWidget)
	{
		return;
	}

	UTexture2D* SkillIcon = ResolveSkillIcon(SlotState.SkillId);
	if (!SkillIcon)
	{
		IconWidget->SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	IconWidget->SetBrushFromTexture(SkillIcon);
	IconWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	
	if (SkillKey)
	{
		SkillKey->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UTimeThiefHUDWidget::UpdateSkillCooldownDisplay()
{
	UpdateSkillCooldownSlotDisplay(
		SkillSlot1Index,
		SkillSlot1_Cooldown_ProgressBar,
		SkillSlot1_Cooldown_Text,
		LastSkillSlot1CooldownPercent,
		LastSkillSlot1CooldownSeconds);
	UpdateSkillCooldownSlotDisplay(
		SkillSlot2Index,
		SkillSlot2_Cooldown_ProgressBar,
		SkillSlot2_Cooldown_Text,
		LastSkillSlot2CooldownPercent,
		LastSkillSlot2CooldownSeconds);
}

void UTimeThiefHUDWidget::UpdateSkillCooldownSlotDisplay(
	uint32 SlotIndex,
	UProgressBar* CooldownProgressBar,
	UTextBlock* CooldownText,
	float& LastCooldownPercent,
	int32& LastCooldownSeconds)
{
	const FTimeThiefSkillCooldownState CooldownState = CachedSkillComponent.IsValid()
		? CachedSkillComponent->GetSkillCooldownState(static_cast<int32>(SlotIndex))
		: FTimeThiefSkillCooldownState();
	const bool bCoolingDown = CooldownState.bCoolingDown && CooldownState.RemainingSeconds > 0.0f;
	if (!bCoolingDown)
	{
		if (CooldownProgressBar)
		{
			if (!FMath::IsNearlyZero(LastCooldownPercent))
			{
				CooldownProgressBar->SetPercent(0.0f);
				LastCooldownPercent = 0.0f;
			}
			// if (CooldownProgressBar->GetVisibility() != ESlateVisibility::Hidden)
			// {
			// 	CooldownProgressBar->SetVisibility(ESlateVisibility::Hidden);
			// }
		}
		if (CooldownText)
		{
			if (CooldownText->GetVisibility() != ESlateVisibility::Hidden)
			{
				CooldownText->SetVisibility(ESlateVisibility::Hidden);
			}
		}
		LastCooldownSeconds = 0;
		return;
	}

	const float CooldownPercent = CachedSkillComponent->GetSkillCooldownPercent(static_cast<int32>(SlotIndex));
	if (CooldownProgressBar)
	{
		if (!FMath::IsNearlyEqual(LastCooldownPercent, CooldownPercent, KINDA_SMALL_NUMBER))
		{
			CooldownProgressBar->SetPercent(CooldownPercent);
			LastCooldownPercent = CooldownPercent;
		}
		if (CooldownProgressBar->GetVisibility() != ESlateVisibility::HitTestInvisible)
		{
			CooldownProgressBar->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}

	const int32 DisplaySeconds = FMath::CeilToInt(CooldownState.RemainingSeconds);
	if (CooldownText)
	{
		if (LastCooldownSeconds != DisplaySeconds)
		{
			CooldownText->SetText(FText::AsNumber(DisplaySeconds));
			LastCooldownSeconds = DisplaySeconds;
		}
		if (CooldownText->GetVisibility() != ESlateVisibility::HitTestInvisible)
		{
			CooldownText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}
}

void UTimeThiefHUDWidget::UpdateSavePointCooldownDisplay()
{
	const float CooldownRemaining = CachedSavePointSkillComponent.IsValid()
		? CachedSavePointSkillComponent->GetRemainingCoolTime()
		: 0.0f;
	const bool bCoolingDown = CooldownRemaining > 0.0f;
	const float CooldownProgress = bCoolingDown
		? 1.0f - CachedSavePointSkillComponent->GetCooldownPercent()
		: 1.0f;

	if (SavePoint_Cooldown_ProgressBar)
	{
		if (!FMath::IsNearlyEqual(LastSavePointCooldownPercent, CooldownProgress, KINDA_SMALL_NUMBER))
		{
			SavePoint_Cooldown_ProgressBar->SetPercent(CooldownProgress);
			LastSavePointCooldownPercent = CooldownProgress;
		}
	}

	const float IconOpacity = bCoolingDown ? 0.35f : 1.0f;
	if (SavePoint_Icon && !FMath::IsNearlyEqual(LastSavePointIconOpacity, IconOpacity, KINDA_SMALL_NUMBER))
	{
		SavePoint_Icon->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, IconOpacity));
		LastSavePointIconOpacity = IconOpacity;
	}
}

UTexture2D* UTimeThiefHUDWidget::ResolveSkillIcon(int32 SkillId) const
{
	const EItemID SkillItemId = ResolveSkillItemId(SkillId);
	if (SkillItemId == EItemID::SIZE)
	{
		return nullptr;
	}

	const UItemSettings* ItemSettings = GetDefault<UItemSettings>();
	UGameItemData* LoadedData = ItemSettings ? ItemSettings->ItemData.LoadSynchronous() : nullptr;
	if (!LoadedData)
	{
		return nullptr;
	}

	const FItemData* ItemData = LoadedData->Items.Find(SkillItemId);
	return ItemData ? ItemData->Icon : nullptr;
}
