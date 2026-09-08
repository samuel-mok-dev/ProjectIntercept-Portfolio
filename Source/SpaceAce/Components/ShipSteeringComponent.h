// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ShipSteeringComponent.generated.h"

class AShipBase;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SPACEACE_API UShipSteeringComponent : public UActorComponent
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	UShipSteeringComponent();

	void SteerTowardsLocation(const FVector& WorldLocation);
	void SteerTowardsDirection(const FVector& WorldDirection);
	void StopSteering();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Strength of correction toward the desired direction
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering")
	float ProportionalGain = 0.6f;

	// Strength of the braking applied against the existing rotation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering")
	float DampingGain = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering")
	float MaximumSteeringInput = 1.0f;

private:
	UPROPERTY()
	TObjectPtr<AShipBase> OwningShip;

};
