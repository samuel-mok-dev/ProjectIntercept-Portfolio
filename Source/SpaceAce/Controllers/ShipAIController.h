// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "ShipAIState.h"
#include "ShipAIController.generated.h"

class AShipBase;
class FShipAIState;
class FShipAIPatrolState;
class FShipAIPursueState;
class FShipAISearchState;
class FShipAIBreakOffState;
class UShipSensingComponent;
struct FShipAIContext;
struct FPerceivedContact;

UCLASS()
class SPACEACE_API AShipAIController : public AAIController
{
	GENERATED_BODY()

public:
	AShipAIController();

	UFUNCTION(BlueprintPure, Category = "AI Debug")
	FString GetCurrentStateName() const;

	UFUNCTION(BlueprintPure, Category = "AI Debug")
	AActor* GetSelectedContactActor() const;

	void ResetForRespawn();

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void OnUnPossess() override;
	virtual ~AShipAIController() override;

private:
	// Reference to the controlled ship
	UPROPERTY()
	TObjectPtr<AShipBase> ControlledShip;

	void EvaluateState();
	void TransitionToState(FShipAIState* NewState);

	const FPerceivedContact* ChooseBestContact() const;

	TUniquePtr<FShipAIPatrolState> PatrolState;
	TUniquePtr<FShipAIPursueState> PursueState;
	TUniquePtr<FShipAISearchState> SearchState;
	TUniquePtr<FShipAIBreakOffState> BreakOffState;

	FShipAIState* CurrentState = nullptr;

	UPROPERTY()
	TObjectPtr<UShipSensingComponent> SensingComponent;

	float DecisionUpdateInterval = 0.1f;
	float TimeSinceLastDecision = 0.0f;

	float BreakOffDistance = 10000.0f;
	float ReEngageDistance = 30000.0f;
};
