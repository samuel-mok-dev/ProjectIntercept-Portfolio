#include "HealthComponent.h"

UHealthComponent::UHealthComponent()
{
    PrimaryComponentTick.bCanEverTick = false; // Disable ticking for this component
}

void UHealthComponent::InitializeHealth(float NewMaxHealth)
{
    MaxHealth = FMath::Max(0.0f, NewMaxHealth);
    CurrentHealth = MaxHealth;
    bIsDead = false;
}

void UHealthComponent::ApplyDamage(float DamageAmount)
{
    if (bIsDead || DamageAmount <= 0.0f)
    {
        return;
    }

    CurrentHealth = FMath::Clamp(
        CurrentHealth - DamageAmount,
        0.0f,
        MaxHealth
    );

    OnDamageReceived.Broadcast(DamageAmount);

    if (CurrentHealth <= 0.0f)
    {
        bIsDead = true;
    }
}

void UHealthComponent::Heal(float HealAmount)
{
    if (bIsDead || HealAmount <= 0.0f)
    {
        return;
    }

    CurrentHealth = FMath::Clamp(
        CurrentHealth + HealAmount,
        0.0f,
        MaxHealth
    );
}

void UHealthComponent::ResetHealth()
{
    CurrentHealth = MaxHealth;
    bIsDead = false;
}

float UHealthComponent::GetCurrentHealth() const
{
    return CurrentHealth;
}

float UHealthComponent::GetMaxHealth() const
{
    return MaxHealth;
}

bool UHealthComponent::IsDead() const
{
    return bIsDead;
};