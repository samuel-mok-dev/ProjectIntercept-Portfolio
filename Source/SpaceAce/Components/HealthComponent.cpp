#include "HealthComponent.h"
#include "Net/UnrealNetwork.h"

UHealthComponent::UHealthComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = false; // Disable ticking for this component
}

void UHealthComponent::InitializeHealth(float NewMaxHealth)
{
    if (GetOwner() && !GetOwner()->HasAuthority()) return;
    MaxHealth = FMath::IsFinite(NewMaxHealth) ? FMath::Max(0.0f, NewMaxHealth) : 0.0f;
    CurrentHealth = MaxHealth;
    bIsDead = false;
}

void UHealthComponent::ApplyDamage(float DamageAmount)
{
    if (GetOwner() && !GetOwner()->HasAuthority()) return;
    if (bIsDead || !FMath::IsFinite(DamageAmount) || DamageAmount <= 0.0f)
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
    if (GetOwner() && !GetOwner()->HasAuthority()) return;
    if (bIsDead || !FMath::IsFinite(HealAmount) || HealAmount <= 0.0f)
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
    if (GetOwner() && !GetOwner()->HasAuthority()) return;
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
void UHealthComponent::OnRep_CurrentHealth(float PreviousHealth)
{
    if (PreviousHealth > CurrentHealth) OnDamageReceived.Broadcast(PreviousHealth - CurrentHealth);
}
void UHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UHealthComponent, CurrentHealth);
    DOREPLIFETIME(UHealthComponent, MaxHealth);
    DOREPLIFETIME(UHealthComponent, bIsDead);
}
