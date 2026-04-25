#include "Components/TimeThiefHealthComponent.h"

#include "Character/TimeThiefCharacterBase.h"
#include "Character/TimeThiefPlayerState.h"
#include "GameFramework/Character.h"

UTimeThiefHealthComponent::UTimeThiefHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	MaxHealth = DefaultMaxHealth;
	CurrentHealth = DefaultMaxHealth;
}

void UTimeThiefHealthComponent::OnEndRespawn()
{
	bIsDead = false;
	if (const ATimeThiefPlayerState* PS = Cast<ATimeThiefPlayerState>(Cast<ACharacter>(GetOwner())->GetPlayerState()))
	{
		MaxHealth = DefaultMaxHealth + PS->Status.Health * UpgradeAmount;
		CurrentHealth = MaxHealth;
		OnHealthChanged_Delegate.Broadcast(this, CurrentHealth, CurrentHealth, nullptr);
	}
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

void UTimeThiefHealthComponent::SetHealth(float MaxHP, float NewHP)
{
	MaxHealth = MaxHP;
	CurrentHealth = FMath::Clamp(NewHP, 0.0f, MaxHealth);
	
	OnHealthChanged_Delegate.Broadcast(this, CurrentHealth, CurrentHealth, nullptr);
}

void UTimeThiefHealthComponent::HandleHealthChanged(float NewHealth, float DeltaHealth)
{
	float ExpectedHealth = GetCurrentHealth() + DeltaHealth;
	if (!FMath::IsNearlyEqual(ExpectedHealth, NewHealth))
	{
		// 경고만 한다
		UE_LOG(LogTemp, Warning, TEXT("Health desync detected: CurrentHealth=%f, DeltaHealth=%f, ExpectedHealth=%f, NewHealth=%f"), GetCurrentHealth(), DeltaHealth, ExpectedHealth, NewHealth);
	}
	
	float CurrHp = GetCurrentHealth();
	float Delta = NewHealth - CurrHp;
	
	if (FMath::IsNearlyEqual(Delta, 0.0f))
	{
		return;
	}
	
	if (Delta < 0.0f)
	{
		TakeDamage(-Delta, nullptr);
	}
	else
	{
		Heal(Delta, nullptr);
	}
}

void UTimeThiefHealthComponent::Upgrade()
{
	if (const ATimeThiefPlayerState* PS = Cast<ATimeThiefPlayerState>(Cast<ACharacter>(GetOwner())->GetPlayerState()))
	{
		MaxHealth = DefaultMaxHealth + PS->Status.Health * UpgradeAmount;
		CurrentHealth += UpgradeAmount;
		OnHealthChanged_Delegate.Broadcast(this, CurrentHealth, CurrentHealth, nullptr);
	}
}

void UTimeThiefHealthComponent::HandleDeath()
{
	if (bIsDead)
	{
		return;
	}

	bIsDead = true;
	OnDeath.Broadcast();
}
