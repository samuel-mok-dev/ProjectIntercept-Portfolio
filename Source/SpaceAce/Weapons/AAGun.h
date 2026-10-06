#pragma once
#include "CoreMinimal.h"
#include "DefenseTurretBase.h"
#include "AAGun.generated.h"

class ALaserProjectile;
class UMaterialInterface;

UCLASS()
class SPACEACE_API AAAGun : public ADefenseTurretBase
{
    GENERATED_BODY()
public:
    AAAGun();
protected:
    virtual void FireAtTarget(AShipBase* Target) override;
    UPROPERTY(EditAnywhere, Category="Weapon") TSubclassOf<ALaserProjectile> ProjectileClass;
    UPROPERTY(EditAnywhere, Category="Weapon") float ProjectileSpeed = 100000.0f;
    UPROPERTY(EditAnywhere, Category="Weapon") float ProjectileDamage = 10.0f;
    UPROPERTY(EditDefaultsOnly, Category="Weapon|Team Effects") TObjectPtr<UMaterialInterface> RoyalNavyLaserMaterial;
    UPROPERTY(EditDefaultsOnly, Category="Weapon|Team Effects") TObjectPtr<UMaterialInterface> ImperialLaserMaterial;
};
