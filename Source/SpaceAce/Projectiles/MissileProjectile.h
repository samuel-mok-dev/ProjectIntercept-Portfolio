// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SphereComponent.h"
#include "NiagaraComponent.h"
#include "ProjectileNetState.h"
#include "MissileProjectile.generated.h"

class AMissileProjectile;
class UAudioComponent;
class AShipBase;

DECLARE_DELEGATE_OneParam(FOnMissileDeactivated, AMissileProjectile*);

UCLASS()
class SPACEACE_API AMissileProjectile : public AActor
{
	GENERATED_BODY()

public:
	AMissileProjectile();

	FOnMissileDeactivated OnMissileDeactivated;

    virtual void Tick(float DeltaTime) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void InitializeProjectile(
		float NewSpeed,
		float NewDamage,
		float NewLifetime
	);

	void ConfigureFlight(
		bool bNewIsHomingMissile,
		const FVector& NewInheritedVelocity,
		float NewGravityScale
	);

	void ActivateProjectile(
		const FVector& SpawnLocation,
		const FRotator& SpawnRotation,
		AActor* NewOwner,
		float NewDamage
	);

	void DeactivateProjectile();

	// Set homing target for missile tracking
	void SetTarget(AActor* NewTarget);
    void ConfigurePayload(float TurnRate, float TrackingAlignment, float Radius, float CapitalDamageMultiplier);
    void Detonate(AActor* DirectHit, const FVector& Location);

	bool IsAvailable() const
	{
		return !bIsActive;
	}

protected:
	virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UNiagaraComponent> MissileEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MissileMesh;

    UPROPERTY(VisibleAnywhere, Category="Effects")
    TObjectPtr<UStaticMeshComponent> MotorGlow;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Homing")
	float HomingTurnRate = 3.0f; // Direction-vector change per second, before responsiveness.
    UPROPERTY(EditDefaultsOnly, Category="Homing", meta=(ClampMin="0",ClampMax="1"))
    float GuidanceResponsiveness = .6f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Homing")
	float MinimumTrackingAlignment = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects")
	TObjectPtr<UNiagaraSystem> MissileExplosionEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
	TObjectPtr<UAudioComponent> MissileAudioComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
	TObjectPtr<USoundBase> MissileLaunchSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
	TObjectPtr<USoundBase> MissileMotorSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
	TObjectPtr<USoundBase> MissileExplosionSound;

	UFUNCTION()
	void OnLaunchSoundFinished();

private:
    UPROPERTY(ReplicatedUsing=OnRep_NetState)
    FProjectileNetState NetState;
    uint32 AppliedRevision = 0;
    UFUNCTION()
    void OnRep_NetState();
    UFUNCTION(NetMulticast, Reliable)
    void MulticastProjectileState(const FProjectileNetState& State);
    UFUNCTION(NetMulticast, Unreliable)
    void MulticastImpact(FVector_NetQuantize Location);
    void PublishNetState();
    void ApplyNetState(const FProjectileNetState& State);
    bool bIsActive = false;

	float ActiveTime = 0.0f;
	float Speed = 40000.0f;  
	float MaxLifetime = 8.0f;  
	float Damage = 50.0f;  
    float BlastRadius = 0.0f;
    float AntiShipDamageMultiplier = 1.0f;
	bool bIsHomingMissile = true;
	float GravityScale = 0.0f;
	FVector InheritedVelocity = FVector::ZeroVector;
	FVector CurrentVelocity = FVector::ZeroVector;

	// Homing target
    UPROPERTY(ReplicatedUsing=OnRep_HomingTarget)
	TObjectPtr<AActor> HomingTarget = nullptr;
    UFUNCTION()
    void OnRep_HomingTarget();

	// Warning Target
	TWeakObjectPtr<AShipBase> WarnedTarget;

	bool bWarningRegistered = false;

	void ClearWarningTarget();
};
