#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SphereComponent.h"
#include "NiagaraComponent.h"
#include "Components/AudioComponent.h"
#include "ProjectileNetState.h"
#include "LaserProjectile.generated.h"

class USphereComponent;
class UNiagaraComponent;
class UProjectileMovementComponent;
class UMaterialInterface;
class ALaserProjectile;

DECLARE_DELEGATE_OneParam(FOnLaserDeactivated, ALaserProjectile*);

UCLASS()
class SPACEACE_API ALaserProjectile : public AActor
{
	GENERATED_BODY()

public:
	ALaserProjectile();

	FOnLaserDeactivated OnLaserDeactivated;

    virtual void Tick(float DeltaTime) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void SetLaserMaterial(UMaterialInterface* NewMaterial);

	void InitializeProjectile(
		float NewSpeed,
		float NewDamage,
		float NewLifetime
	);

	void ActivateProjectile(
	const FVector& SpawnLocation,
	const FRotator& SpawnRotation,
	AActor* NewOwner,
	float NewDamage,
	float NewSpeed
	);

	void DeactivateProjectile();

	bool IsAvailable() const
	{
		return !bIsActive;
	}

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UNiagaraComponent> LaserEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects")
	TObjectPtr<UNiagaraSystem> HitSparkEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
	TObjectPtr<USoundBase> HitSparkSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
	TObjectPtr<USoundBase> LaserFiredSound;

	UFUNCTION()
	void HandleHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		FVector NormalImpulse,
		const FHitResult& Hit
	);

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
	float Speed = 1000000.0f;
	float MaxLifetime = 1.0f;
	float Damage = 10.0f;

    UPROPERTY(ReplicatedUsing=OnRep_LaserMaterial)
	TObjectPtr<UMaterialInterface> LaserMaterial;
    UFUNCTION()
    void OnRep_LaserMaterial();
};
