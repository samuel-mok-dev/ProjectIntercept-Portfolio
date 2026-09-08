#include "ShipAIController.h"
#include "ShipAIController.h"
#include "ShipBase.h"
#include "ShipSensingComponent.h"
#include "ShipAIState.h"

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
            Error,
            TEXT("%s: ControlledShip is null in BeginPlay."),
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
    Super::Tick(DeltaTime);

    if (!ControlledShip || !CurrentState)
    {
        return;
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
    Context.DeltaTime = DeltaTime;
    Context.CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    CurrentState->Update(Context);
}

const FPerceivedContact* AShipAIController::ChooseBestContact() const
{
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
        if (!Contact.Actor.IsValid())
        {
            continue;
        }

        const float DistanceSquared = FVector::DistSquared(
            ControlledShip->GetActorLocation(),
            Contact.LastKnownLocation
        );

        if (DistanceSquared < ClosestDistanceSquared)
        {
            ClosestDistanceSquared = DistanceSquared;
            BestContact = &Contact;
        }
    }

    return BestContact;

}

void AShipAIController::EvaluateState()
{
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
    if (ControlledShip)
    {
        ControlledShip->StopAIControl();
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
    return CurrentState
        ? CurrentState->GetStateName()
        : TEXT("None");
}

AActor* AShipAIController::GetSelectedContactActor() const
{
    const FPerceivedContact* Contact = ChooseBestContact();

    return Contact && Contact->Actor.IsValid()
        ? Contact->Actor.Get()
        : nullptr;
}

void AShipAIController::ResetForRespawn()
{
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