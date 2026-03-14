#include "UI/TimeThiefHUDWidget.h"
#include "Character/TimeThiefPlayerCharacter.h"
#include "Components/HorizontalBox.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/TimeThiefHealthComponent.h"
#include "Components/Combat/TimeThiefPlayerCombatComponent.h"
#include "Components/System/TimePointSystemComponent.h"
#include "Weapon/TimeThiefRifle.h"

void UTimeThiefHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	if (ATimeThiefPlayerCharacter* PlayerChar = Cast<ATimeThiefPlayerCharacter>(GetOwningPlayerPawn()))
	{
		InitializeHUD(PlayerChar);
	}
}

void UTimeThiefHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (CachedCharacter.IsValid())
	{
		UpdateAmmoDisplay();
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

void UTimeThiefHUDWidget::OnTimePointUpdated(int DisplayTimePoints)
{
	TimePoint_Text->SetText(FText::AsNumber(DisplayTimePoints));
}

void UTimeThiefHUDWidget::UpdateAmmoDisplay()
{
	if (CachedCombatComponent.IsValid())
	{
		if (ATimeThiefRifle* Rifle = Cast<ATimeThiefRifle>(CachedCombatComponent->GetCharacterCurrentEquippedWeapon()))
		{
			OnAmmoUpdated(Rifle->GetCurrentAmmo(), Rifle->GetMaxAmmo(), true);
			return;
		}
	}
	
	OnAmmoUpdated(0, 0, false);
}

void UTimeThiefHUDWidget::UpdateCrosshairDisplay()
{
	if (CachedCombatComponent.IsValid())
	{
		const bool bIsAiming = CachedCombatComponent->IsAiming();
		float SpreadMultiplier = 1.0f;

		if (bIsAiming)
		{
			SpreadMultiplier *= CachedCombatComponent->GetAimSpreadMultiplier();
		}

		if (CachedCombatComponent->IsFiringWeapon())
		{
			SpreadMultiplier *= 2.0f; 
		}

		OnCrosshairSpreadUpdated(SpreadMultiplier, bIsAiming);
	}
}