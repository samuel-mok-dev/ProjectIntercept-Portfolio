#pragma once
#include "CoreMinimal.h"
#include "DefenseTurretBase.h"
#include "SAMTurret.generated.h"

class AMissileProjectile;

UCLASS()
class SPACEACE_API ASAMTurret : public ADefenseTurretBase
{
    GENERATED_BODY()
protected:
    virtual void FireAtTarget(AShipBase* Target) override;
    UPROPERTY(EditAnywhere, Category="Weapon") TSubclassOf<AMissileProjectile> ProjectileClass;
    UPROPERTY(EditAnywhere, Category="Weapon") float ProjectileSpeed = 9000.0f;
    UPROPERTY(EditAnywhere, Category="Weapon") float ProjectileDamage = 50.0f;
    UPROPERTY(EditAnywhere, Category="Weapon") float ProjectileLifetime = 8.0f;

private:
    bool bFireFromPrimaryMuzzleNext = true;
};
