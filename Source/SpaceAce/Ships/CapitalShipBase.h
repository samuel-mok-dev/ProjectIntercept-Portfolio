#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MissionObjectiveInterface.h"
#include "CapitalShipBase.generated.h"

class ADefenseTurretBase;
class UAudioComponent;
class UBoxComponent;
class UCapitalShipDataAsset;
class UNiagaraComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class ECapitalShipMovementState : uint8
{
    Holding,
    MovingToLocation,
    JumpingIn,
    AligningForJumpOut,
    JumpingOut
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCapitalShipManeuverCompleted);

UCLASS()
class SPACEACE_API ACapitalShipBase : public AActor, public IMissionObjectiveInterface
{
	GENERATED_BODY()

public:
	ACapitalShipBase();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;
	virtual bool IsMissionObjectiveComplete_Implementation() const override;

	UFUNCTION(BlueprintCallable, Category = "Capital Ship")
	void ApplyCapitalShipData();

    const TArray<TObjectPtr<AActor>>& GetMissionObjectives() const { return ObjectiveActors; }
    int32 GetExpectedMissionObjectiveCount() const;

	UFUNCTION(BlueprintCallable, Category = "Capital Ship")
	void SetTeamID(int32 NewTeamID);

	UFUNCTION(BlueprintPure, Category = "Capital Ship")
	int32 GetTeamID() const { return TeamID; }

	UFUNCTION(BlueprintCallable, Category = "Movement")
	void MoveToLocation(const FVector& NewTargetLocation);

	UFUNCTION(BlueprintCallable, Category = "Movement")
	void HoldPosition();

	UFUNCTION(BlueprintCallable, Category = "Movement")
	void JumpIn(
		const FVector& ArrivalLocation,
		const FVector& ArrivalDirection
	);

	UFUNCTION(BlueprintCallable, Category = "Movement")
	void JumpOut(const FVector& ExitDirection);

	UPROPERTY(BlueprintAssignable, Category = "Movement")
	FOnCapitalShipManeuverCompleted OnManeuverCompleted;

	UFUNCTION(BlueprintImplementableEvent, Category = "Hyperspace")
	void OnJumpInStarted();

	UFUNCTION(BlueprintImplementableEvent, Category = "Hyperspace")
	void OnJumpInFinished();

	UFUNCTION(BlueprintImplementableEvent, Category = "Hyperspace")
	void OnJumpOutStarted();

	UFUNCTION(BlueprintImplementableEvent, Category = "Hyperspace")
	void OnJumpOutFinished();

protected:
    virtual bool ShouldSpawnWeapons() const { return true; }
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ShipMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> CollisionBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UAudioComponent> EngineAudio;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship Data")
	TObjectPtr<UCapitalShipDataAsset> CapitalShipData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Capital Ship")
	int32 TeamID = 0;

	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Weapons")
	TArray<TObjectPtr<ADefenseTurretBase>> SpawnedWeapons;

	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Objectives")
	TArray<TObjectPtr<AActor>> ObjectiveActors;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraComponent>> EngineEffectComponents;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Movement")
	ECapitalShipMovementState MovementState = ECapitalShipMovementState::Holding;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "1.0"))
	float ArrivalAcceptanceRadius = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hyperspace", meta = (ClampMin = "0.0"))
	float JumpTransitionDuration = 1.0f;

private:
	void ClearSpawnedAttachments();
	void SpawnWeapons();
	void SpawnObjectives();
	void SpawnEngineEffects();
	void UpdateMovement(float DeltaTime);
	void SteerTowardsLocation(const FVector& Location, float DeltaTime);
	void CompleteManeuver();

	FVector TargetLocation = FVector::ZeroVector;
	FVector JumpDirection = FVector::ForwardVector;
	FVector JumpArrivalLocation = FVector::ZeroVector;
	FVector JumpStartLocation = FVector::ZeroVector;

	float JumpSpeed = 100000.0f;
	float JumpEntryDistance = 25000.0f;
	float JumpExitDistance = 25000.0f;
	float JumpOutAlignmentTolerance = 1.0f;
	float ForwardSpeed = 1000.0f;
	float Acceleration = 100.0f;
	float RotationSpeed = 10.0f;
	float CurrentSpeed = 0.0f;
	float CurrentHealth = 5000.0f;
	bool bDestroyed = false;
};
