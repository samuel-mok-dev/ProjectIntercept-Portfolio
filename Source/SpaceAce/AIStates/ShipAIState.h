#pragma once

#include "CoreMinimal.h"

class AShipBase;
class UShipSensingComponent;
struct FPerceivedContact;

struct FShipAIContext
{
    AShipBase* ControlledShip = nullptr;
    UShipSensingComponent* SensingComponent = nullptr;

    const FPerceivedContact* SelectedContact = nullptr;

    float DeltaTime = 0.0f;
    float CurrentTime = 0.0f;
};

class FShipAIState
{
public:
    virtual ~FShipAIState() = default;

    virtual void Enter(FShipAIContext& Context) = 0;
    virtual void Update(FShipAIContext& Context) = 0;
    virtual void Exit(FShipAIContext& Context) = 0;
    virtual FString GetStateName() const = 0;
};

class FShipAIPatrolState : public FShipAIState
{
public:
    virtual void Enter(FShipAIContext& Context) override;
    virtual void Update(FShipAIContext& Context) override;
    virtual void Exit(FShipAIContext& Context) override;
    virtual FString GetStateName() const override { return TEXT("Patrol"); }
    FVector GetPatrolCentre() const { return HomeLocation; }
    float GetPatrolRadius() const { return PatrolDistance; }
    FVector GetPatrolDirection() const { return PatrolDirection; }

private:
    void ChooseNewPatrolDirection(FShipAIContext& Context);

    FVector HomeLocation = FVector::ZeroVector;
    bool bHomeLocationInitialized = false;
    FVector PatrolDirection = FVector::ZeroVector;

    float TimeUntilNewPatrolHeading = 0.0f;
    float PatrolHeadingDuration = 4.0f;

    float PatrolDistance = 15000.0f;

    float PatrolAcceptanceRadius = 3000.0f;
};

// Time-based duty cycle: frame rate and rapid trigger re-presses cannot bypass pauses.
struct FShipAIFireDiscipline
{
    float Age = 0.f;
    float Reaction = .75f;
    float Burst = .8f;
    float Pause = 1.1f;
    void Reset(float NewReaction = .75f, float NewBurst = .8f, float NewPause = 1.1f)
    { Age=0.f; Reaction=NewReaction; Burst=NewBurst; Pause=NewPause; }
    void Advance(float DeltaTime) { Age += FMath::Max(0.f, DeltaTime); }
    bool IsReady() const { return Age >= Reaction; }
    bool CanFireGuns() const
    { return IsReady() && FMath::Fmod(Age-Reaction, Burst+Pause) < Burst; }
};

class FShipAIPursueState : public FShipAIState
{
public:
    virtual void Enter(FShipAIContext& Context) override;
    virtual void Update(FShipAIContext& Context) override;
    virtual void Exit(FShipAIContext& Context) override;
    virtual FString GetStateName() const override { return TEXT("Pursue"); }

private:
    FShipAIFireDiscipline FireDiscipline;
    TWeakObjectPtr<AActor> EngagedTarget;
    float AimErrorPhase = 0.f;
    float GunAlignmentThreshold = 0.95f;
    float MissileAlignmentThreshold = 0.9f;
    float MissileFireCooldown = 8.0f;
    float TimeSinceLastMissile = 0.0f;
    float MissileProximityLimit = 7000.0f;
};

class FShipAIBreakOffState : public FShipAIState
{
public:
    virtual void Enter(FShipAIContext& Context) override;
    virtual void Update(FShipAIContext& Context) override;
    virtual void Exit(FShipAIContext& Context) override;
    virtual FString GetStateName() const override { return TEXT("BreakOff"); }

private:
    FVector BreakOffDirection = FVector::ZeroVector;
};

class FShipAISearchState : public FShipAIState
{
public:
    virtual void Enter(FShipAIContext& Context) override;
    virtual void Update(FShipAIContext& Context) override;
    virtual void Exit(FShipAIContext& Context) override;
    virtual FString GetStateName() const override { return TEXT("Search"); }

private:
    FVector SearchLocation = FVector::ZeroVector;
};
