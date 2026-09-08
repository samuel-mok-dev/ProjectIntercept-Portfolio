#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnDamageReceived, 
    float, 
    DamageAmount
);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SPACEACE_API UHealthComponent : public UActorComponent
{
    GENERATED_BODY()

    protected:
        UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health")
        float MaxHealth = 100.0f;

        UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Health")
        float CurrentHealth = 0.0f;

        bool bIsDead = false;

    public:
        UHealthComponent();

        void InitializeHealth(float NewMaxHealth);
        void ApplyDamage(float DamageAmount);
        void Heal(float HealAmount);
        void ResetHealth();

        float GetCurrentHealth() const;
        float GetMaxHealth() const;

        bool IsDead() const;

        UPROPERTY(BlueprintAssignable, Category = "Health")
        FOnDamageReceived OnDamageReceived;
};