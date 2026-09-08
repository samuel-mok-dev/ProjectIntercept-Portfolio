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

    void Configure(const FGameplayTag& NewEnemyTeamTag);
    void AcquireTargets();
    void SwitchTarget();
    void UpdateLockOn(float DeltaTime);

    bool IsLockedOn() const;

    AActor* GetCurrentTarget() const;

protected:
    UPROPERTY()
    TObjectPtr<AActor> CurrentTarget;

    UPROPERTY()
    TArray<TObjectPtr<AActor>> SortedTargets;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
    FGameplayTag EnemyTeamTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
    float MinimumLockOnDotProduct = 0.85f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Targeting")
    float RequiredLockOnTime = 1.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Targeting")
    float LockOnTime = 0.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Targeting")
    bool bIsLockedOn = false;

    int32 CurrentTargetIndex = INDEX_NONE;

private:
    bool IsValidTarget(AActor* Candidate) const;

    void SelectTarget(int32 TargetIndex);
    void ClearTarget();
};