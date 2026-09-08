#include "TargetingComponent.h"
#include "GameplayTagAssetInterface.h"
#include "EngineUtils.h"
#include "ShipBase.h"

UTargetingComponent::UTargetingComponent()
{
    PrimaryComponentTick.bCanEverTick = false; // Disable ticking for this component
}

void UTargetingComponent::Configure(const FGameplayTag& NewEnemyTeamTag)
{
    EnemyTeamTag = NewEnemyTeamTag;

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("%s: Targeting configured with enemy tag: %s"),
        *GetNameSafe(GetOwner()),
        *EnemyTeamTag.ToString()
    );
}

bool UTargetingComponent::IsValidTarget(AActor* Candidate) const
{
    if (!IsValid(Candidate) || Candidate == GetOwner())
    {
        return false;
    }

    AShipBase* CandidateShip = Cast<AShipBase>(Candidate);

    if (CandidateShip && CandidateShip->IsDead())
    {
        return false;
    }

    IGameplayTagAssetInterface* TagInterface = 
    Cast<IGameplayTagAssetInterface>(Candidate);

    if (!TagInterface)
    {
        return false;
    }

    FGameplayTagContainer CandidateTags;
    TagInterface->GetOwnedGameplayTags(CandidateTags);

    const bool bHasEnemyTag =
    CandidateTags.HasTag(EnemyTeamTag);

    return bHasEnemyTag;
}

void UTargetingComponent:: AcquireTargets()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        SortedTargets.Reset();
        CurrentTarget = nullptr;
        CurrentTargetIndex = INDEX_NONE;
        return;
    }

    const TWeakObjectPtr<AActor> PreviousTarget =
        CurrentTarget;

    SortedTargets.Reset();

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Candidate = *It;

        if (IsValidTarget(Candidate))
        {
            SortedTargets.Add(Candidate);
        }
    }

    // Sort targets by distance from this component's owner
    AActor* Owner = GetOwner();

    if (!Owner)
    {
        return;
    }

    const FVector OwnerLocation = Owner->GetActorLocation();

    SortedTargets.Sort(
        [OwnerLocation](const AActor& A, const AActor& B)
        {
            const float DistanceA = 
            FVector::DistSquared(OwnerLocation, A.GetActorLocation());
            
            const float DistanceB = 
            FVector::DistSquared(OwnerLocation, B.GetActorLocation());

            return DistanceA < DistanceB;
        }
    );

    if (SortedTargets.IsEmpty())
    {
        ClearTarget();
        return;
    }

    if (PreviousTarget.IsValid() &&
        IsValidTarget(PreviousTarget.Get()))
    {
        const int32 PreviousTargetIndex =
            SortedTargets.IndexOfByPredicate(
                [&PreviousTarget](const TObjectPtr<AActor>& Candidate)
                {
                    return Candidate == PreviousTarget.Get();
                }
            );

        if (PreviousTargetIndex != INDEX_NONE)
        {
            SelectTarget(PreviousTargetIndex);
            return;
        }
    }

    SelectTarget(0); 

}

void UTargetingComponent::SelectTarget(int32 TargetIndex)
{
    if (!SortedTargets.IsValidIndex(TargetIndex))
    {
        ClearTarget();
        return;
    }

    AActor* NewTarget = SortedTargets[TargetIndex];

    const bool bTargetChanged = NewTarget != CurrentTarget;

    CurrentTargetIndex = TargetIndex;
    CurrentTarget = SortedTargets[CurrentTargetIndex];

    if (bTargetChanged)
    {
        LockOnTime = 0.0f;
        bIsLockedOn = false;
    }
    
    UE_LOG(
        LogTemp,
        Warning,
        TEXT("%s Selected Target: %s"),
        *GetNameSafe(GetOwner()),
        *GetNameSafe(CurrentTarget)
    );
}

void UTargetingComponent::ClearTarget()
{
    CurrentTarget = nullptr;
    CurrentTargetIndex = INDEX_NONE;

    LockOnTime = 0.0f;
    bIsLockedOn = false;
}

void UTargetingComponent::SwitchTarget()
{
    UE_LOG(
        LogTemp,
        Warning,
        TEXT("%s: SwitchTarget called | Targets: %d | Current index: %d"),
        *GetNameSafe(GetOwner()),
        SortedTargets.Num(),
        CurrentTargetIndex
    );
    
    if (SortedTargets.IsEmpty())
    {
        AcquireTargets();
    }

    if (SortedTargets.IsEmpty())
    {
        ClearTarget();
        return;
    }

    const int32 TargetCount = SortedTargets.Num();

    for (int32 Attempts = 0; Attempts < SortedTargets.Num(); ++Attempts)
    {
        CurrentTargetIndex = 
            (CurrentTargetIndex + 1) % TargetCount;
        
            AActor* Candidate = SortedTargets[CurrentTargetIndex];

            if (IsValidTarget(Candidate))
            {
                SelectTarget(CurrentTargetIndex);
                return;
            }
    }

    ClearTarget();
}

AActor* UTargetingComponent::GetCurrentTarget() const
{
    return CurrentTarget;
}

bool UTargetingComponent::IsLockedOn() const
{
    return bIsLockedOn;
}

void UTargetingComponent::UpdateLockOn(float DeltaTime)
{
    if (!IsValidTarget(CurrentTarget))
    {
        LockOnTime = 0.0f;
        bIsLockedOn = false;
        
        AcquireTargets();

        if (!CurrentTarget)
        {
            return;
        }
    }

    AShipBase* OwnerShip = Cast<AShipBase>(GetOwner());

    if (!OwnerShip)
    {
        LockOnTime = 0.0f;
        bIsLockedOn = false;
        return;
    }

    const FVector DistanceSquaredVector = 
        CurrentTarget->GetActorLocation() - OwnerShip->GetActorLocation();

    if (DistanceSquaredVector.SizeSquared() > FMath::Square(OwnerShip->GetMissileWeaponRange()))
    {
        LockOnTime = 0.0f;
        bIsLockedOn = false;
        return;
    }

    const FVector DirectionToTarget =
        (CurrentTarget->GetActorLocation() - OwnerShip->GetActorLocation())
        .GetSafeNormal();

    const FVector OwnerForward = OwnerShip->GetActorForwardVector();

    const float Alignment =
        FVector::DotProduct(OwnerForward, DirectionToTarget);

    if (Alignment < MinimumLockOnDotProduct)
    {
        LockOnTime = 0.0f;
        bIsLockedOn = false;
        return;
    }

    LockOnTime += DeltaTime;

    if (LockOnTime >= RequiredLockOnTime)
    {
        bIsLockedOn = true;
    }
}