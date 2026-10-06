#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MissionObjectiveInterface.h"
#include "DefenseTurretBase.generated.h"

class AShipBase;
class USceneComponent;
class UStaticMeshComponent;

UCLASS(Abstract)
class SPACEACE_API ADefenseTurretBase : public AActor, public IMissionObjectiveInterface
{
	GENERATED_BODY()

public:
	ADefenseTurretBase();
	virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    bool IsDestroyed() const { return bDestroyed; }
    UFUNCTION() void OnRep_Destroyed();
	virtual void Tick(float DeltaSeconds) override;
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;
	virtual bool IsMissionObjectiveComplete_Implementation() const override { return bDestroyed; }

	UFUNCTION(BlueprintCallable, Category = "Defense")
	void SetTeamID(int32 NewTeamID) { TeamID = NewTeamID; }

	UFUNCTION(BlueprintPure, Category = "Defense")
	int32 GetTeamID() const { return TeamID; }

	UFUNCTION(BlueprintPure, Category = "Turret|Aiming")
	bool CanAimAtLocation(const FVector& WorldLocation) const;

protected:
	virtual void FireAtTarget(AShipBase* Target) PURE_VIRTUAL(ADefenseTurretBase::FireAtTarget, );
	AShipBase* FindTarget() const;
	bool AimAtTarget(const AActor* Target, float DeltaSeconds);
	bool HasLineOfSightTo(const AActor* Target) const;
	bool GetDesiredLocalAim(const FVector& WorldLocation, float& OutYaw, float& OutPitch) const;
	FVector GetMuzzleLocation() const;
	FRotator GetMuzzleRotation() const;
	USceneComponent* GetPrimaryMuzzle() const { return PrimaryMuzzle; }
	USceneComponent* GetSecondaryMuzzle() const { return SecondaryMuzzle; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turret|Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turret|Components")
	TObjectPtr<UStaticMeshComponent> FixedBaseMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turret|Components")
	TObjectPtr<UStaticMeshComponent> YawBaseMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turret|Components")
	TObjectPtr<USceneComponent> PitchAssembly;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turret|Components")
	TObjectPtr<UStaticMeshComponent> WeaponMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turret|Components")
	TObjectPtr<USceneComponent> PrimaryMuzzle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turret|Components")
	TObjectPtr<USceneComponent> SecondaryMuzzle;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = "Defense")
	int32 TeamID = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense", meta = (ClampMin = "0.0"))
	float Range = 30000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense", meta = (ClampMin = "0.01"))
	float FireInterval = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Turret|Constraints", meta = (ClampMin = "-180.0", ClampMax = "180.0"))
	float MinimumYaw = -180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Turret|Constraints", meta = (ClampMin = "-180.0", ClampMax = "180.0"))
	float MaximumYaw = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Turret|Constraints", meta = (ClampMin = "-89.0", ClampMax = "89.0"))
	float MinimumPitch = -10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Turret|Constraints", meta = (ClampMin = "-89.0", ClampMax = "89.0"))
	float MaximumPitch = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Turret|Aiming", meta = (ClampMin = "0.0"))
	float YawSpeedDegreesPerSecond = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Turret|Aiming", meta = (ClampMin = "0.0"))
	float PitchSpeedDegreesPerSecond = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Turret|Aiming", meta = (ClampMin = "0.0"))
	float FireToleranceDegrees = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Turret|Aiming")
	bool bRequireLineOfSight = true;

	UPROPERTY(ReplicatedUsing=OnRep_Destroyed, BlueprintReadOnly, Category = "Defense")
	bool bDestroyed = false;

private:
	UPROPERTY(Replicated) float Health = 100.0f;
    UPROPERTY(Replicated) float ReplicatedYaw = 0;
    UPROPERTY(Replicated) float ReplicatedPitch = 0;
    TWeakObjectPtr<AShipBase> CachedTarget;
    float TargetScanTimer = 0;
	float FireTimer = 0.0f;
};
