#include "TargetingComponent.h"
#include "Net/UnrealNetwork.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "GameplayTagAssetInterface.h"
#include "EngineUtils.h"
#include "ShipBase.h"
#include "DefenseTurretBase.h"
#include "CapitalShipBase.h"
#include "MissileWeaponComponent.h"

UTargetingComponent::UTargetingComponent()
{
    SetIsReplicatedByDefault(true);
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
    if (!IsValid(Candidate) || Candidate == GetOwner() || Candidate->IsHidden())
    {
        return false;
    }

    AShipBase* CandidateShip = Cast<AShipBase>(Candidate);

    if (CandidateShip && (CandidateShip->IsDead() || CandidateShip->bOnCatapult))
    {
        return false;
    }

    if (const AShipBase* OwnerShip = Cast<AShipBase>(GetOwner()))
    {
        if (CandidateShip && OwnerShip->GetTeamID() != INDEX_NONE && CandidateShip->GetTeamID() != INDEX_NONE)
            return OwnerShip->IsHostileTo(CandidateShip);
    }

    if (const auto* Turret=Cast<ADefenseTurretBase>(Candidate))
    {
        const auto* Ship=Cast<AShipBase>(GetOwner());
        return Ship && !Turret->IsDestroyed() && Ship->GetTeamID()!=Turret->GetTeamID();
    }
    if (const auto* Capital = Cast<ACapitalShipBase>(Candidate))
    {
        const auto* Ship = Cast<AShipBase>(GetOwner());
        return Candidate == PriorityTarget.Get() && Ship && !Capital->IsHidden() &&
            Ship->GetTeamID() != INDEX_NONE && Capital->GetTeamID() != INDEX_NONE &&
            Ship->GetTeamID() != Capital->GetTeamID();
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

void UTargetingComponent::SetPriorityTarget(AActor* Target)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || PriorityTarget.Get() == Target) return;
    PriorityTarget = Target;
    AcquireTargets();
}

void UTargetingComponent::SetAICombatTarget(AActor* Target)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    const bool bChanged = !bAIControlsTarget || AICombatTarget.Get() != Target;
    bAIControlsTarget = true;
    AICombatTarget = Target;
    if (bChanged || !IsValidTarget(CurrentTarget)) AcquireTargets();
}

void UTargetingComponent::ReleaseAICombatTarget()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    bAIControlsTarget = false;
    AICombatTarget.Reset();
    ClearTarget();
    AcquireTargets();
}

void UTargetingComponent:: AcquireTargets()
{
    if (GetOwner() && !GetOwner()->HasAuthority()) return;
    if (const auto* Ship=Cast<AShipBase>(GetOwner()); Ship && Ship->bOnCatapult) { ClearTarget(); return; }
    TRACE_CPUPROFILER_EVENT_SCOPE(SpaceAce_UTargetingComponent_AcquireTargets);
    UWorld* World = GetWorld();
    if (!World)
    {
        SortedTargets.Reset();
        ClearTarget();
        return;
    }

    const TWeakObjectPtr<AActor> PreviousTarget =
        CurrentTarget;

    SortedTargets.Reset();

    if (IsValidTarget(PriorityTarget.Get()))
    {
        SortedTargets.Add(PriorityTarget.Get());
        if (CurrentTarget != PriorityTarget.Get()) SelectTarget(0);
        else CurrentTargetIndex = 0;
        return;
    }

    if (bAIControlsTarget)
    {
        if (IsValidTarget(AICombatTarget.Get()))
        {
            SortedTargets.Add(AICombatTarget.Get());
            if (CurrentTarget != AICombatTarget.Get()) SelectTarget(0);
            else CurrentTargetIndex = 0;
        }
        else ClearTarget();
        return;
    }

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
    if (GetOwner() && !GetOwner()->HasAuthority()) return;
    if (const auto* Ship=Cast<AShipBase>(GetOwner()); Ship && Ship->bOnCatapult) { ClearTarget(); return; }
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
    const auto* Ship=Cast<AShipBase>(GetOwner());
    return Ship && Ship->bOnCatapult ? nullptr : CurrentTarget;
}

bool UTargetingComponent::IsLockedOn() const
{
    return bIsLockedOn;
}

float UTargetingComponent::GetLockOnProgress() const
{
    if (bIsLockedOn) return 1.0f;
    const auto* Weapon=GetOwner() ? GetOwner()->FindComponentByClass<UMissileWeaponComponent>() : nullptr;
    const float Duration=Weapon ? Weapon->GetRequiredLockTime() : RequiredLockOnTime;
    return Duration > KINDA_SMALL_NUMBER ? FMath::Clamp(LockOnTime / Duration,0.0f,1.0f) : 0.0f;
}

void UTargetingComponent::ResetLockOn()
{
    if (GetOwner() && !GetOwner()->HasAuthority()) return;
    if (const auto* Ship=Cast<AShipBase>(GetOwner()); Ship && Ship->bOnCatapult) { ClearTarget(); return; }
    LockOnTime = 0.0f;
    bIsLockedOn = false;
}

void UTargetingComponent::UpdateLockOn(float DeltaTime)
{
    if (GetOwner() && !GetOwner()->HasAuthority()) return;
    if (const auto* Ship=Cast<AShipBase>(GetOwner()); Ship && Ship->bOnCatapult) { ClearTarget(); return; }
    TRACE_CPUPROFILER_EVENT_SCOPE(SpaceAce_UTargetingComponent_UpdateLockOn);
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

	if (!OwnerShip || OwnerShip->IsDead() || !OwnerShip->IsCombatEnabled() ||
        OwnerShip->IsInPreMatchFlight() || !OwnerShip->IsCurrentSecondaryWeaponHoming())
    {
        LockOnTime = 0.0f;
        bIsLockedOn = false;
        return;
    }
    if (const auto* Weapon=OwnerShip->FindComponentByClass<UMissileWeaponComponent>())
    {
        RequiredLockOnTime=Weapon->GetRequiredLockTime();
        if (!Weapon->IsTargetCompatible(CurrentTarget)) { LockOnTime=0;bIsLockedOn=false;return; }
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

    LockOnTime = FMath::Min(LockOnTime + FMath::Max(DeltaTime, 0.0f),
        FMath::Max(RequiredLockOnTime, 0.0f));

    if (LockOnTime >= RequiredLockOnTime)
    {
        bIsLockedOn = true;
    }
}
void UTargetingComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(UTargetingComponent, CurrentTarget, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(UTargetingComponent, LockOnTime, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(UTargetingComponent, bIsLockedOn, COND_OwnerOnly);
}
