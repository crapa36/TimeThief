#include "UI/TimeThiefHUDWidget.h"
#include "Character/TimeThiefPlayerCharacter.h"
#include "Components/TimeThiefHealthComponent.h"
#include "Components/Combat/TimeThiefPlayerCombatComponent.h"
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

	if (CachedHealthComponent.IsValid())
	{
		CachedHealthComponent->OnHealthChanged.AddDynamic(this, &UTimeThiefHUDWidget::HandleHealthChanged);
		HandleHealthChanged(CachedHealthComponent.Get(), CachedHealthComponent->GetCurrentHealth(), CachedHealthComponent->GetCurrentHealth(), nullptr);
	}
}

void UTimeThiefHUDWidget::HandleHealthChanged(UTimeThiefHealthComponent* HealthComp, float OldHealth, float NewHealth, AActor* Instigator)
{
	if (HealthComp)
	{
		OnHealthUpdated(NewHealth, HealthComp->GetMaxHealth(), HealthComp->GetHealthPercent());
	}
}

void UTimeThiefHUDWidget::UpdateAmmoDisplay()
{
	if (CachedCombatComponent.IsValid())
	{
		if (ATimeThiefRifle* Rifle = Cast<ATimeThiefRifle>(CachedCombatComponent->GetCharacterCurrentEquippedWeapon()))
		{
			OnAmmoUpdated(Rifle->GetCurrentAmmo(), Rifle->GetReserveAmmo(), true);
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