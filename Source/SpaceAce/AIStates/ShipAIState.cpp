#include "ShipAIState.h"
#include "ShipBase.h"
#include "ShipSensingComponent.h"

void FShipAIPatrolState::Enter(FShipAIContext& Context)
{
    UE_LOG(LogTemp, Warning, TEXT("AI entered Patrol state"));
    
    if (!Context.ControlledShip)
    {
        return;
    }

    if (!bHomeLocationInitialized)
    {
        HomeLocation = Context.ControlledShip->GetActorLocation();
        bHomeLocationInitialized = true;
    }

    ChooseNewPatrolDirection(Context);
}

void FShipAIPatrolState::Update(
	FShipAIContext& Context
)
{
	if (!Context.ControlledShip)
	{
		return;
	}

	TimeUntilNewPatrolHeading -= Context.DeltaTime;

    const FVector CurrentLocation = Context.ControlledShip->GetActorLocation();

    const float DistanceFromHome = FVector::Dist(CurrentLocation, HomeLocation);

    const bool bIsOutsidePatrolRadius = DistanceFromHome > PatrolDistance;

    if (bIsOutsidePatrolRadius)
    {
        PatrolDirection = (HomeLocation - CurrentLocation).GetSafeNormal();
    }
    else if (TimeUntilNewPatrolHeading <= 0.0f)
	{
		ChooseNewPatrolDirection(Context);
	}

	Context.ControlledShip->SetThrottleInput(0.0f);

	Context.ControlledShip->SteerTowardsDirection(
		PatrolDirection
	);
}

void FShipAIPatrolState::ChooseNewPatrolDirection(
	FShipAIContext& Context
)
{
	if (!Context.ControlledShip)
	{
		return;
	}

	FVector LevelForward =
		Context.ControlledShip->GetActorForwardVector();

	LevelForward.Z = 0.0f;

	if (!LevelForward.Normalize())
	{
		LevelForward = FVector::ForwardVector;
	}

	FVector RandomOffset = FMath::VRand();
	RandomOffset.Z *= 0.1f;
	RandomOffset *= 0.2f;

	PatrolDirection =
		(LevelForward + RandomOffset).GetSafeNormal();

	TimeUntilNewPatrolHeading =
		PatrolHeadingDuration;
}


void FShipAIPatrolState::Exit(FShipAIContext& Context)
{
}

void FShipAIPursueState::Enter(FShipAIContext& Context)
{
    UE_LOG(LogTemp, Warning, TEXT("AI entered Pursue state"));
}

void FShipAIPursueState::Update(FShipAIContext& Context)
{
    if (
        !Context.ControlledShip ||
        !Context.SelectedContact ||
        !Context.SelectedContact->Actor.IsValid()
    )
    {
        return;
    }

    TimeSinceLastMissile += Context.DeltaTime;

    AActor* TargetActor = Context.SelectedContact->Actor.Get();

    const FVector ShipLocation = Context.ControlledShip->GetActorLocation();
    const FVector TargetLocation = TargetActor->GetActorLocation();
    const FVector DirectionToTarget = (TargetLocation - ShipLocation).GetSafeNormal();

    const FVector TargetVelocity = TargetActor->GetVelocity();

    const float DistanceToTarget = FVector::Distance(ShipLocation, TargetLocation);

    const float LaserProjectileSpeed =
        Context.ControlledShip->GetLaserProjectileSpeed();
    
    FVector AimLocation = TargetLocation;

    if (LaserProjectileSpeed > KINDA_SMALL_NUMBER)
    {
        const float EstimatedTravelTime =
            DistanceToTarget / LaserProjectileSpeed;
        
        AimLocation = 
            TargetLocation +
            TargetVelocity * EstimatedTravelTime;
    }

    const FVector DirectionToAimPoint = 
        (AimLocation - ShipLocation).GetSafeNormal();


    const float ForwardAlignment = FVector::DotProduct(
        Context.ControlledShip->GetShipForwardVector(),
        DirectionToAimPoint
    );

    Context.ControlledShip->SetThrottleInput(0.0f);

    Context.ControlledShip->SteerTowardsLocation(AimLocation);

    const float LaserRange =
        Context.ControlledShip->GetLaserWeaponRange();

    const bool bCanFireLasers =
        ForwardAlignment >= GunAlignmentThreshold &&
        DistanceToTarget <= LaserRange;
    
    if (bCanFireLasers)
    {
        Context.ControlledShip->StartFiringLasers();
    }

    else
    {
        Context.ControlledShip->StopFiringLasers();
    }

    const float MissileRange =
        Context.ControlledShip->GetMissileWeaponRange();

    const bool bCanFireMissile = (
        DistanceToTarget >= MissileProximityLimit &&
        ForwardAlignment >= MissileAlignmentThreshold &&
        TimeSinceLastMissile >= MissileFireCooldown &&
        !Context.ControlledShip->GetHasMissileInTheAir());

    if (
        bCanFireMissile &&
        Context.ControlledShip->GetIsLockedOn() &&
        Context.ControlledShip->HasLoadedMissile()
    )
    {
        Context.ControlledShip->FireSingleMissile();
        TimeSinceLastMissile = 0.0f;
    }
}

void FShipAIPursueState::Exit(FShipAIContext& Context)
{
    if (!Context.ControlledShip)
    {
        return;
    }

    Context.ControlledShip->StopFiringLasers();
}

void FShipAIBreakOffState::Enter(FShipAIContext& Context)
{
    if (!Context.ControlledShip)
    {
        return;
    }

    FVector Forward = Context.ControlledShip->GetActorForwardVector();

    FVector Up = Context.ControlledShip->GetActorUpVector();

    FVector Right = Context.ControlledShip->GetActorRightVector();

    BreakOffDirection = (Forward + Up *0.25f).GetSafeNormal();

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("%s: AI entered BreakOff state"),
        *GetNameSafe(Context.ControlledShip)
    );
}

void FShipAIBreakOffState::Update(FShipAIContext& Context)
{
    if (!Context.ControlledShip)
    {
        return;
    }

    Context.ControlledShip->SetThrottleInput(0.0f);

    Context.ControlledShip->SteerTowardsDirection(BreakOffDirection);
}

void FShipAIBreakOffState::Exit(FShipAIContext& Context)
{
}

void FShipAISearchState::Enter(FShipAIContext& Context)
{
    UE_LOG(LogTemp, Warning, TEXT("AI entered Search state"));
    
    if (!Context.ControlledShip || !Context.SelectedContact)
    {
        return;
    }

    SearchLocation = Context.SelectedContact->LastKnownLocation;
}

void FShipAISearchState::Update(FShipAIContext& Context)
{
    if (!Context.ControlledShip)
    {
        return;
    }

    Context.ControlledShip->SteerTowardsLocation(SearchLocation);
}

void FShipAISearchState::Exit(FShipAIContext& Context)
{
}