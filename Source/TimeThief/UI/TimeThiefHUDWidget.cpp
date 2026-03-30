#include "UI/TimeThiefHUDWidget.h"
#include "Character/TimeThiefPlayerCharacter.h"
#include "Components/HorizontalBox.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/TimeThiefHealthComponent.h"
#include "Components/Combat/TimeThiefPlayerCombatComponent.h"
#include "Components/System/TimePointSystemComponent.h"
#include "Weapon/Components/TimeThiefWeaponComponentBase.h"

void UTimeThiefHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	if (Crosshair_Image)
	{
		Crosshair_Image->SetVisibility(ESlateVisibility::Hidden);
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
	Super::NativeDestruct();
}

void UTimeThiefHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (CachedCharacter.IsValid())
	{
		UpdateCrosshairDisplay();
	}
}

void UTimeThiefHUDWidget::InitializeHUD(ATimeThiefPlayerCharacter* InCharacter)
{
	if (!InCharacter) return;

	CachedCharacter = InCharacter;
	CachedHealthComponent = InCharacter->GetComponentByClass<UTimeThiefHealthComponent>();
	CachedCombatComponent = InCharacter->GetPlayerCombatComponent();
	CachedTimePointSystemComponent = InCharacter->GetComponentByClass<UTimePointSystemComponent>();
	
	if (CachedHealthComponent.IsValid())
	{
		CachedHealthComponent->OnHealthChanged_Delegate.AddUObject(this, &UTimeThiefHUDWidget::OnHealthUpdated);
		OnHealthUpdated(CachedHealthComponent.Get(), 100, 100, nullptr);
	}
	
	if (CachedTimePointSystemComponent.IsValid())
	{
		CachedTimePointSystemComponent->OnTimePointsChanged_Delegate.AddUObject(this, &UTimeThiefHUDWidget::OnTimePointUpdated);
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

	if (Crosshair_Image)
	{
		Crosshair_Image->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

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

	if (Crosshair_Image)
	{
		Crosshair_Image->SetVisibility(ESlateVisibility::Hidden);
	}

	OnAmmoUpdated(0, 0, false);
}

void UTimeThiefHUDWidget::OnTimePointUpdated(int DisplayTimePoints)
{
	TimePoint_Text->SetText(FText::AsNumber(DisplayTimePoints));
}

void UTimeThiefHUDWidget::UpdateCrosshairDisplay()
{
	if (CachedCombatComponent.IsValid() && Crosshair_Image && CachedWeapon.IsValid())
	{
		float SpreadMultiplier = 1.0f + (CachedWeapon->GetCurrentSpread() * 0.5f);

		Crosshair_Image->SetRenderScale(FVector2D(SpreadMultiplier, SpreadMultiplier));
	}
}