#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "TargetingComponent.generated.h"

class AActor;
class UCombatantSubsystem;
class AShipBase;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SPACEACE_API UTargetingComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UTargetingComponent();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    void Configure(const FGameplayTag& NewEnemyTeamTag);
    void AcquireTargets();
    void SwitchTarget();
    void UpdateLockOn(float DeltaTime);
    void ResetLockOn();
    // Mission AI orders may override normal nearest-enemy acquisition.
    void SetPriorityTarget(AActor* Target);
    // AI owns ordinary target selection, including an intentional absence of a contact.
    void SetAICombatTarget(AActor* Target);
    void ReleaseAICombatTarget();

    bool IsLockedOn() const;

    UFUNCTION(BlueprintPure, Category = "Targeting")
    float GetLockOnProgress() const;

    AActor* GetCurrentTarget() const;

protected:
    UPROPERTY(Replicated)
    TObjectPtr<AActor> CurrentTarget;

    UPROPERTY()
    TArray<TObjectPtr<AActor>> SortedTargets;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
    FGameplayTag EnemyTeamTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
    float MinimumLockOnDotProduct = 0.85f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Targeting")
    float RequiredLockOnTime = 1.0f;

    UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Targeting")
    float LockOnTime = 0.0f;

    UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Targeting")
    bool bIsLockedOn = false;

    int32 CurrentTargetIndex = INDEX_NONE;

private:
    TWeakObjectPtr<AActor> PriorityTarget;
    TWeakObjectPtr<AActor> AICombatTarget;
    bool bAIControlsTarget = false;
    bool IsValidTarget(AActor* Candidate) const;

    void SelectTarget(int32 TargetIndex);
    void ClearTarget();
};
