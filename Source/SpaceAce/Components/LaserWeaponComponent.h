#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Materials/MaterialInterface.h"
#include "LaserWeaponComponent.generated.h"

class ALaserProjectile;
class USceneComponent;
class AShipBase;
class UMaterialInterface;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SPACEACE_API ULaserWeaponComponent : public UActorComponent
{
    GENERATED_BODY()

protected:
    // Blueprint editor properties for the laser weapon component
    UPROPERTY()
    TArray<TObjectPtr<ALaserProjectile>> LaserPool;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laser Weapon")
    int32 LaserPoolSize = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laser Weapon")
    float LaserDamage = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laser Weapon")
    float LaserSpeed = 20000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laser Weapon")
    float LaserLifetime = 3.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Laser Weapon")
    TSubclassOf<ALaserProjectile> LaserProjectileClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laser Weapon")
    TArray<USceneComponent*> CannonMuzzles;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> LaserMaterial;

    FTimerHandle FireTimerHandle;
    TQueue<ALaserProjectile*> AvailableLasers;

public:
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLaserFired);
    FOnLaserFired OnLaserFired;

    float DamageMultiplier = 1.0f;
    float SpeedMultiplier = 1.0f;
    static constexpr float BaseGunFireInterval = 0.2f;
    float GunFireRate = 1.0f;

    bool bIsFiring = false;

    ULaserWeaponComponent();

    void Configure(
        const TArray<USceneComponent*>& NewCannonMuzzles,
        float NewDamageMultiplier,
        float NewSpeedMultiplier,
        float NewGunFireRate,
        int32 NewLaserPoolSize,
        TSubclassOf<ALaserProjectile> NewLaserProjectileClass,
        UMaterialInterface* NewLaserMaterial
    );
    void InitializeLaserPool();
    void FireLaser();
    void StartFiringLasers();
    void StopFiringLasers();

    ALaserProjectile* GetAvailableLaser();

    void ReturnLaserToPool(ALaserProjectile* Laser);

    // Getters
    float GetEffectiveRange() const;
    float GetProjectileSpeed() const;
};