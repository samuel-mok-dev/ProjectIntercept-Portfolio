#include "ShipSteeringComponent.h"
#include "ShipBase.h"

// Sets default values
UShipSteeringComponent::UShipSteeringComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// Called when the game starts or when spawned
void UShipSteeringComponent::BeginPlay()
{
	Super::BeginPlay();

	OwningShip = Cast<AShipBase>(GetOwner());

	if (!OwningShip)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("%s: must be attached to an AShipBase actor!"),
			*GetNameSafe(this)
		);
	}
	
}

void UShipSteeringComponent::SteerTowardsLocation(const FVector& WorldLocation)
{
	if (!OwningShip)
	{
		return;
	}

	const FVector ToTarget = 
	WorldLocation - OwningShip->GetActorLocation();

	if (ToTarget.IsNearlyZero())
	{
		StopSteering();
		return;
	}

	SteerTowardsDirection(ToTarget);
}

void UShipSteeringComponent::SteerTowardsDirection(const FVector& WorldDirection)
{
	if (!OwningShip || WorldDirection.IsNearlyZero())
	{
		return;
	}

	const FVector DesiredDirection = WorldDirection.GetSafeNormal();

	const FVector ShipForward = OwningShip->GetShipForwardVector();

	const FVector ShipRight = OwningShip->GetShipRightVector();

	const FVector ShipUp = OwningShip->GetShipUpVector();

	const float ForwardAlignment =
		FVector::DotProduct(ShipForward, DesiredDirection);
	
	const float RightAlignment =
		FVector::DotProduct(ShipRight, DesiredDirection);

	const float UpAlignment =
		FVector::DotProduct(ShipUp, DesiredDirection);

	const float YawError = FMath::Atan2(RightAlignment, ForwardAlignment);

	const float PitchError = FMath::Atan2(UpAlignment, ForwardAlignment);

	const float HorizontalAlignment = FMath::Sqrt(
		FMath::Square(ForwardAlignment) +
		FMath::Square(RightAlignment)
	);

	const FVector AngularVelocity = OwningShip->GetShipAngularVelocity();

	const float CurrentYawVelocity = FVector::DotProduct(
		AngularVelocity,
		ShipUp
	);

	const float CurrentPitchVelocity = FVector::DotProduct(
		AngularVelocity,
		ShipRight
	);

	float YawInput = 
		ProportionalGain * YawError - 
		DampingGain * CurrentYawVelocity;
	
	float PitchInput = 
		-ProportionalGain * PitchError - 
		DampingGain * CurrentPitchVelocity;

	YawInput = FMath::Clamp(
		YawInput,
		-MaximumSteeringInput,
		MaximumSteeringInput
	);

	PitchInput = FMath::Clamp(
		PitchInput,
		-MaximumSteeringInput,
		MaximumSteeringInput
	);

	OwningShip->SetYawInput(YawInput);
	OwningShip->SetPitchInput(PitchInput);
	OwningShip->SetRollInput(0.0f);

}

void UShipSteeringComponent::StopSteering()
{
	if (!OwningShip)
	{
		return;
	}

	OwningShip->SetYawInput(0.0f);
	OwningShip->SetPitchInput(0.0f);
	OwningShip->SetRollInput(0.0f);
}