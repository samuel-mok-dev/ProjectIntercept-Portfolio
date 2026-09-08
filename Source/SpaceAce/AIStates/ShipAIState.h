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

class FShipAIPursueState : public FShipAIState
{
public:
    virtual void Enter(FShipAIContext& Context) override;
    virtual void Update(FShipAIContext& Context) override;
    virtual void Exit(FShipAIContext& Context) override;
    virtual FString GetStateName() const override { return TEXT("Pursue"); }

private:
    float GunAlignmentThreshold = 0.95f;
    float MissileAlignmentThreshold = 0.9f;
    float MissileFireCooldown = 2.0f;
	float TimeSinceLastMissile = 1000.0f;
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