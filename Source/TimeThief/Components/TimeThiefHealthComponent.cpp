#include "Components/TimeThiefHealthComponent.h"

UTimeThiefHealthComponent::UTimeThiefHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	MaxHealth = DefaultMaxHealth;
	CurrentHealth = DefaultMaxHealth;
}

void UTimeThiefHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	InitializeWithHealth(DefaultMaxHealth);

	if (AActor* Owner = GetOwner())
	{
		Owner->OnTakeAnyDamage.AddDynamic(this, &UTimeThiefHealthComponent::OnTakeAnyDamageCallback);
	}
}

void UTimeThiefHealthComponent::OnTakeAnyDamageCallback(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	AActor* Instigator = InstigatedBy ? InstigatedBy->GetPawn() : DamageCauser;
	TakeDamage(Damage, Instigator);
}

void UTimeThiefHealthComponent::InitializeWithHealth(float InMaxHealth)
{
	MaxHealth = InMaxHealth;
	CurrentHealth = MaxHealth;
	bIsDead = false;
}

float UTimeThiefHealthComponent::GetHealthPercent() const
{
	if (MaxHealth <= 0.0f)
	{
		return 0.0f;
	}
	return CurrentHealth / MaxHealth;
}

void UTimeThiefHealthComponent::TakeDamage(float DamageAmount, AActor* DamageInstigator)
{
	if (bIsDead || DamageAmount <= 0.0f)
	{
		return;
	}

	const float OldHealth = CurrentHealth;
	CurrentHealth = FMath::Clamp(CurrentHealth - DamageAmount, 0.0f, MaxHealth);
	LastDamageInstigator = DamageInstigator;

	OnHealthChanged_Delegate.Broadcast(this, OldHealth, CurrentHealth, DamageInstigator);

	if (CurrentHealth <= 0.0f)
	{
		HandleDeath();
	}
}

void UTimeThiefHealthComponent::Heal(float HealAmount, AActor* HealInstigator)
{
	if (bIsDead || HealAmount <= 0.0f)
	{
		return;
	}

	const float OldHealth = CurrentHealth;
	CurrentHealth = FMath::Clamp(CurrentHealth + HealAmount, 0.0f, MaxHealth);

	OnHealthChanged_Delegate.Broadcast(this, OldHealth, CurrentHealth, HealInstigator);
}

void UTimeThiefHealthComponent::HandleDeath()
{
	if (bIsDead)
	{
		return;
	}

	bIsDead = true;
	OnDeath.Broadcast(GetOwner());
}