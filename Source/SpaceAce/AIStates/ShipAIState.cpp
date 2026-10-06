#include "ShipAIState.h"
#include "ShipBase.h"
#include "LaserWeaponComponent.h"
#include "ShipSensingComponent.h"
#include "SkirmishManager.h"

void FShipAIPatrolState::Enter(FShipAIContext& Context)
{
    UE_LOG(LogTemp, Warning, TEXT("AI entered Patrol state"));
    
    if (!Context.ControlledShip)
    {
        return;
    }

    auto* Manager=Context.ControlledShip->GetSkirmishManager();
    if (Manager && Manager->GetCombatPatrolArea(HomeLocation,PatrolDistance))
    {
        bHomeLocationInitialized=true;
    }
    else if (!bHomeLocationInitialized)
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
    EngagedTarget.Reset();
    FireDiscipline.Reset();
    UE_LOG(LogTemp, Warning, TEXT("AI entered Pursue state"));
}

void FShipAIPursueState::Update(FShipAIContext& Context)
{
    if (
        !Context.ControlledShip ||
        !Context.SelectedContact ||
        !Context.SelectedContact->Actor.IsValid() ||
        !Context.SelectedContact->bCurrentlyVisible
    )
    {
        if (Context.ControlledShip)
        {
            Context.ControlledShip->StopFiringLasers();
            if (auto* Guns=Context.ControlledShip->FindComponentByClass<ULaserWeaponComponent>())
                Guns->SetAIAimError(FVector2D::ZeroVector);
        }
        EngagedTarget.Reset();
        return;
    }

    TimeSinceLastMissile += Context.DeltaTime;

    AActor* TargetActor = Context.SelectedContact->Actor.Get();
    if (EngagedTarget.Get() != TargetActor)
    {
        EngagedTarget = TargetActor;
        FireDiscipline.Reset(FMath::FRandRange(.5f,1.f), FMath::FRandRange(.6f,1.f), FMath::FRandRange(.8f,1.5f));
        AimErrorPhase = FMath::FRandRange(0.f,2.f*PI);
        Context.ControlledShip->StopFiringLasers();
    }
    else FireDiscipline.Advance(Context.DeltaTime);

    // Shared, smooth angular error for both muzzles; steering cannot cancel it by
    // following the erroneous point. This affects AI weapon aim only.
    if (auto* Guns=Context.ControlledShip->FindComponentByClass<ULaserWeaponComponent>())
    {
        const float Phase = Context.CurrentTime*2.2f + AimErrorPhase;
        Guns->SetAIAimError(FVector2D(.7f*FMath::Sin(Phase), .7f*FMath::Sin(Phase*.73f+1.1f)));
    }

    const FVector ShipLocation = Context.ControlledShip->GetActorLocation();
    const FVector TargetLocation = TargetActor->GetActorLocation();
    const FVector DirectionToTarget = (TargetLocation - ShipLocation).GetSafeNormal();

    const FVector TargetVelocity = TargetActor->GetVelocity();

    const float DistanceToTarget = FVector::Distance(ShipLocation, TargetLocation);

    const float LaserProjectileSpeed =
        Context.ControlledShip->GetLaserProjectileSpeed();
    
    const FVector AimLocation = ULaserWeaponComponent::PredictIntercept(
        ShipLocation, TargetLocation, TargetVelocity, LaserProjectileSpeed,
        Context.ControlledShip->GetLaserWeaponRange() / FMath::Max(LaserProjectileSpeed, 1.f));

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
        FireDiscipline.CanFireGuns() &&
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
        FireDiscipline.IsReady() &&
        DistanceToTarget >= MissileProximityLimit &&
        DistanceToTarget <= MissileRange &&
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
    if (auto* Guns=Context.ControlledShip->FindComponentByClass<ULaserWeaponComponent>())
        Guns->SetAIAimError(FVector2D::ZeroVector);
    EngagedTarget.Reset();
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
