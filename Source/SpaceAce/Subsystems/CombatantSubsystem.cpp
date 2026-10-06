#include "CombatantSubsystem.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

void UCombatantSubsystem::RegisterCombatant(AActor* Combatant)
{
    if (!IsValid(Combatant)
    )
    {
        return;
    }

    RegisteredCombatants.Add(Combatant);

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("%s: registered with CombatantSubsystem. Total: %d"),
        *GetNameSafe(Combatant),
        RegisteredCombatants.Num()
    );
}

void UCombatantSubsystem::UnregisterCombatant(AActor* Combatant)
{
    if (!IsValid(Combatant))
    {
        return;
    }

    const int32 RemovedCount = RegisteredCombatants.Remove(Combatant);

    if (RemovedCount <= 0)
    {
        return;
    }

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("%s: unregistered from CombatantSubsystem. Total: %d"),
        *GetNameSafe(Combatant),
        RegisteredCombatants.Num()
    );
}

void UCombatantSubsystem::GetCombatantsInRange(
    const FVector& Origin,
    float Radius,
    TArray<AActor*>& OutCombatants
)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(SpaceAce_UCombatantSubsystem_GetCombatantsInRange);
    OutCombatants.Reset();

    if (Radius <= 0.0f)
    {
        return;
    }

    RemoveInvalidCombatants();

    const float RadiusSquared = FMath::Square(Radius);

    for (const TWeakObjectPtr<AActor>& CombatantPointer : RegisteredCombatants)
    {
        AActor* Combatant = CombatantPointer.Get();
    
        if (!IsValid(Combatant))
        {
            continue;
        }

        const float DistanceSquared = FVector::DistSquared(
            Origin,
            Combatant->GetActorLocation()
        );

        if (DistanceSquared <= RadiusSquared)
        {
            OutCombatants.Add(Combatant);
        }
    }
}

int32 UCombatantSubsystem::GetRegisteredCombatantCount() const
{
    return RegisteredCombatants.Num();
}

void UCombatantSubsystem::RemoveInvalidCombatants()
{
    for (
        auto Iterator = RegisteredCombatants.CreateIterator();
        Iterator;
        ++Iterator
    )
    {
        if (!Iterator->IsValid())
        {
            Iterator.RemoveCurrent();
        }
    }
}