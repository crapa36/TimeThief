#include "UI/TimeThiefHUDWidget.h"
#include "Character/TimeThiefPlayerCharacter.h"
#include "Components/HorizontalBox.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/TimeThiefHealthComponent.h"
#include "Components/Combat/TimeThiefPlayerCombatComponent.h"
#include "Components/System/TimePointSystemComponent.h"
#include "Components/Wire/TimeThiefWireComponent.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "UI/TimeThiefControlGuideWidget.h"
#include "Weapon/Components/TimeThiefWeaponComponentBase.h"

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

	if (CachedTimePointSystemComponent.IsValid())
	{
		CachedTimePointSystemComponent->OnTimePointsChanged_Delegate.RemoveAll(this);
	}

	CachedWeapon.Reset();
	CachedWireComponent.Reset();

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
		if (CachedWeapon.IsValid())
		{
			CachedWeapon->OnAmmoChanged_Delegate.RemoveAll(this);
		}
	}

	CachedCharacter = InCharacter;
	CachedHealthComponent = InCharacter->GetComponentByClass<UTimeThiefHealthComponent>();
	CachedCombatComponent = InCharacter->GetPlayerCombatComponent();
	CachedWireComponent = InCharacter->GetWireComponent();
	CachedTimePointSystemComponent = InCharacter->GetComponentByClass<UTimePointSystemComponent>();
	
	if (CachedHealthComponent.IsValid())
	{
		CachedHealthComponent->OnHealthChanged_Delegate.AddUObject(this, &UTimeThiefHUDWidget::OnHealthUpdated);
		// OnHealthUpdated(CachedHealthComponent.Get(), 100, 100, nullptr);
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
