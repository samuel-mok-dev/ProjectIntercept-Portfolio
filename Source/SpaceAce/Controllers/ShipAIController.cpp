#include "ShipAIController.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "ShipAIController.h"
#include "ShipBase.h"
#include "ShipSensingComponent.h"
#include "ShipAIState.h"
#include "TargetingComponent.h"
#include "Components/PrimitiveComponent.h"
#include "MissileWeaponComponent.h"
#include "LaserWeaponComponent.h"
#include "SceneTransitionSubsystem.h"

AShipAIController::AShipAIController()
{
    PrimaryActorTick.bCanEverTick = true;

    PatrolState = MakeUnique<FShipAIPatrolState>();
    PursueState = MakeUnique<FShipAIPursueState>();
    SearchState = MakeUnique<FShipAISearchState>();
    BreakOffState = MakeUnique<FShipAIBreakOffState>();
}

AShipAIController::~AShipAIController() = default;

void AShipAIController::BeginPlay()
{
    Super::BeginPlay();

    if (!ControlledShip)
    {
        ControlledShip = Cast<AShipBase>(GetPawn());
    }

    if (!ControlledShip)
    {
        UE_LOG(
            LogTemp,
            Verbose,
            TEXT("%s: Awaiting possession in BeginPlay."),
            *GetNameSafe(this)
        );

        return;
    }

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("%s acquired target: %s"),
        *GetNameSafe(ControlledShip),
        *GetNameSafe(ControlledShip->GetCurrentTarget())
    );
}

void AShipAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    ControlledShip = Cast<AShipBase>(InPawn);

    if (!ControlledShip)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("%s: Failed to possess ship. InPawn is not of type AShipBase."),
            *GetNameSafe(this)
        );

        return;
    }

    SensingComponent = ControlledShip->GetSensingComponent();
    if (auto* Targeting = ControlledShip->FindComponentByClass<UTargetingComponent>())
        Targeting->SetAICombatTarget(nullptr);

    FShipAIContext Context;
    Context.ControlledShip = ControlledShip;
    Context.SensingComponent = SensingComponent;
    Context.CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    CurrentState = PatrolState.Get();

    if(CurrentState)
    {
        CurrentState->Enter(Context);
    }

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("%s: Successfully possessed ship: %s"),
        *GetNameSafe(this),
        *GetNameSafe(ControlledShip)
    );

    AActor* Target = ControlledShip->GetCurrentTarget();

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("%s: Current target after possession: %s"),
        *GetNameSafe(this),
        *GetNameSafe(Target)
    );
}

void AShipAIController::Tick(float DeltaTime)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(SpaceAce_AShipAIController_Tick);
    Super::Tick(DeltaTime);

    if (!USceneTransitionSubsystem::IsSceneReady(this) || !ControlledShip || !CurrentState || ControlledShip->bOnCatapult ||
        ControlledShip->IsDead() || !ControlledShip->IsCombatEnabled())
    {
        return;
    }

    if (MissionAttackTarget.IsValid() && !MissionAttackTarget->IsHidden())
    {
        UpdateMissionAttack(DeltaTime);
        return;
    }
    if (auto* Weapon=ControlledShip->FindComponentByClass<UMissileWeaponComponent>())
    {
        Weapon->SetWeaponMode(ESecondaryWeaponMode::StandardMissile);
    }

    TimeSinceLastDecision += DeltaTime;

    if (TimeSinceLastDecision >= DecisionUpdateInterval)
    {
        TimeSinceLastDecision = 0.0f;

        EvaluateState();
    }

    FShipAIContext Context;
    Context.ControlledShip = ControlledShip;
    Context.SensingComponent = SensingComponent;
    Context.SelectedContact = ChooseBestContact();
    if (auto* Targeting = ControlledShip->FindComponentByClass<UTargetingComponent>())
        Targeting->SetAICombatTarget(Context.SelectedContact && Context.SelectedContact->bCurrentlyVisible
            ? Context.SelectedContact->Actor.Get() : nullptr);
    Context.DeltaTime = DeltaTime;
    Context.CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    CurrentState->Update(Context);
}

const FPerceivedContact* AShipAIController::ChooseBestContact() const
{
    TRACE_CPUPROFILER_EVENT_SCOPE(SpaceAce_AShipAIController_ChooseBestContact);
    if (!SensingComponent || !ControlledShip)
    {
        return nullptr;
    }

    const TArray<FPerceivedContact>& Contacts = 
        SensingComponent->GetPerceivedContacts();

    const FPerceivedContact* BestContact = nullptr;
    float ClosestDistanceSquared = TNumericLimits<float>::Max();

    for (const FPerceivedContact& Contact : Contacts)
    {
        const auto* Fighter = Cast<AShipBase>(Contact.Actor.Get());
        if (!IsValid(Fighter) || Fighter->IsDead() || Fighter->IsHidden() || Fighter->bOnCatapult ||
            !Fighter->IsCombatEnabled() || !ControlledShip->IsHostileTo(Fighter))
        {
            continue;
        }

        const float DistanceSquared = FVector::DistSquared(
            ControlledShip->GetActorLocation(),
            Contact.LastKnownLocation
        );

        if (!BestContact || (Contact.bCurrentlyVisible && !BestContact->bCurrentlyVisible) ||
            (Contact.bCurrentlyVisible == BestContact->bCurrentlyVisible && DistanceSquared < ClosestDistanceSquared))
        {
            ClosestDistanceSquared = DistanceSquared;
            BestContact = &Contact;
        }
    }

    return BestContact;

}

void AShipAIController::EvaluateState()
{
    TRACE_CPUPROFILER_EVENT_SCOPE(SpaceAce_AShipAIController_EvaluateState);
    const FPerceivedContact* BestContact = ChooseBestContact();

    if (!BestContact)
    {
        TransitionToState(PatrolState.Get());
        return;
    }

    AActor* TargetActor = BestContact->Actor.Get();

    const float DistanceToTarget = FVector::Distance(
        ControlledShip->GetActorLocation(),
        TargetActor->GetActorLocation()
    );

    if (CurrentState == BreakOffState.Get())
    {
        if (DistanceToTarget < ReEngageDistance)
        {
            return;
        }
    }

    if (BestContact->bCurrentlyVisible && DistanceToTarget <= BreakOffDistance)
    {
        TransitionToState(BreakOffState.Get());
        return;
    }

    if (BestContact->bCurrentlyVisible)
    {
        TransitionToState(PursueState.Get());
        return;
    }
    
    TransitionToState(SearchState.Get());
}

void AShipAIController::TransitionToState(FShipAIState* NewState)
{
    if (!NewState || NewState == CurrentState)
    {
        return;
    }

    FShipAIContext Context;
    Context.ControlledShip = ControlledShip;
    Context.SensingComponent = SensingComponent;
    Context.SelectedContact = ChooseBestContact();
    Context.CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    
    if (CurrentState)
    {
        CurrentState->Exit(Context);
    }

    CurrentState = NewState;
    CurrentState->Enter(Context);
}

void AShipAIController::OnUnPossess()
{
    SetMissionAttackTarget(nullptr);
    if (ControlledShip)
    {
        ControlledShip->StopAIControl();
        if (auto* Targeting = ControlledShip->FindComponentByClass<UTargetingComponent>())
            Targeting->ReleaseAICombatTarget();
    }

    FShipAIContext Context;
    Context.ControlledShip = ControlledShip;
    Context.SensingComponent = SensingComponent;

    if (CurrentState)
    {
        CurrentState->Exit(Context);
    }

    CurrentState = nullptr;
    SensingComponent = nullptr;

    ControlledShip = nullptr;

    Super::OnUnPossess();
}

FString AShipAIController::GetCurrentStateName() const
{
    if (MissionAttackTarget.IsValid()) return bMissionBreakOff ? TEXT("Carrier attack break-off") : TEXT("Attack carrier");
    return CurrentState
        ? CurrentState->GetStateName()
        : TEXT("None");
}

AActor* AShipAIController::GetSelectedContactActor() const
{
    if (MissionAttackTarget.IsValid()) return MissionAttackTarget.Get();
    const FPerceivedContact* Contact = ChooseBestContact();

    return Contact && Contact->Actor.IsValid()
        ? Contact->Actor.Get()
        : nullptr;
}

void AShipAIController::ResetForRespawn()
{
    SetMissionAttackTarget(nullptr);
    if (CurrentState && ControlledShip)
    {
        FShipAIContext ExitContext;
        ExitContext.ControlledShip = ControlledShip;
        ExitContext.SensingComponent = SensingComponent;
        ExitContext.SelectedContact = ChooseBestContact();
        ExitContext.CurrentTime =
            GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

        CurrentState->Exit(ExitContext);
    }
    PatrolState = MakeUnique<FShipAIPatrolState>();
    PursueState = MakeUnique<FShipAIPursueState>();
    SearchState = MakeUnique<FShipAISearchState>();
    BreakOffState = MakeUnique<FShipAIBreakOffState>();

    ControlledShip = Cast<AShipBase>(GetPawn());

    if (!ControlledShip)
    {
        CurrentState = nullptr;
        SensingComponent = nullptr;
        return;
    }

    SensingComponent =
        ControlledShip->GetSensingComponent();

    TimeSinceLastDecision = 0.0f;

    CurrentState = PatrolState.Get();

    FShipAIContext EnterContext;
    EnterContext.ControlledShip = ControlledShip;
    EnterContext.SensingComponent = SensingComponent;
    EnterContext.CurrentTime =
        GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    CurrentState->Enter(EnterContext);

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("%s: AI reset for respawn in Patrol state"),
        *GetNameSafe(ControlledShip)
    );
}

void AShipAIController::SetMissionAttackTarget(AActor* Target)
{
    if (!HasAuthority()) return;
    MissionAttackTarget = Target;
    bMissionBreakOff = false;
    MissionMissileCooldown = 0.0f;
    MissionBombsDropped=0;
    if (ControlledShip)
    {
        if(Target)
        {
            const auto* Hull=Cast<UPrimitiveComponent>(Target->GetRootComponent());
            const FVector Centre=Hull?Hull->Bounds.Origin:Target->GetActorLocation();
            MissionRunDirection=FVector::VectorPlaneProject(Centre-ControlledShip->GetActorLocation(),Target->GetActorUpVector()).GetSafeNormal();
            if(MissionRunDirection.IsNearlyZero())MissionRunDirection=Target->GetActorForwardVector();
        }
        ControlledShip->StopFiringLasers();
        if (auto* Guns=ControlledShip->FindComponentByClass<ULaserWeaponComponent>())
            Guns->SetAIAimError(FVector2D::ZeroVector);
        if (auto* Targeting = ControlledShip->FindComponentByClass<UTargetingComponent>()) Targeting->SetPriorityTarget(Target);
    }
}

void AShipAIController::UpdateMissionAttack(float DeltaTime)
{
    AActor* Target = MissionAttackTarget.Get();
    const FVector Position = ControlledShip->GetActorLocation();
    const auto* Hull = Cast<UPrimitiveComponent>(Target->GetRootComponent());
    const FBox Bounds = Hull ? Hull->Bounds.GetBox() : FBox(Target->GetActorLocation(), Target->GetActorLocation());
    auto* Weapon=ControlledShip->FindComponentByClass<UMissileWeaponComponent>();
    ControlledShip->StopFiringLasers();
    if(!Weapon || Weapon->GetSpecialWeaponProfile().Type!=ESpecialWeaponType::UnguidedBomb)
    {SetMissionAttackTarget(nullptr);return;}
    Weapon->SetWeaponMode(ESecondaryWeaponMode::SpecialWeapon);
    const FVector Up=Target->GetActorUpVector();
    const FVector Centre=Bounds.GetCenter();
    const float Along=FVector::DotProduct(Position-Centre,MissionRunDirection);
    const float HalfLength=FVector::DotProduct(Bounds.GetExtent(),MissionRunDirection.GetAbs());
    // A straight corridor above the hull: never chase the carrier's pivot or its defenses.
    const FVector AimPoint=Centre+MissionRunDirection*(Along+25000)+Up*(Bounds.GetExtent().Z+6000);
    ControlledShip->SetThrottleInput(0.0f);
    ControlledShip->SteerTowardsLocation(AimPoint);
    if(!bMissionBreakOff && Weapon->GetSpecialWeaponAmmo()>0 && MissionBombsDropped<2)
    {
        FVector Impact;
        if(Weapon->HasLoadedMissile() && Weapon->PredictBombImpact(Impact,Target))
        {
            const int32 Before=Weapon->GetSpecialWeaponAmmo();
            ControlledShip->FireSingleMissile();
            if(Weapon->GetSpecialWeaponAmmo()<Before)++MissionBombsDropped;
        }
    }
    bMissionBreakOff=MissionBombsDropped>=2 || Weapon->GetSpecialWeaponAmmo()==0 || Along>HalfLength;
    if(Along>HalfLength+15000)
    {
        SetMissionAttackTarget(nullptr);
        Weapon->SetWeaponMode(ESecondaryWeaponMode::StandardMissile);
        TimeSinceLastDecision=DecisionUpdateInterval;
    }
}
