#pragma once

#include "CoreMinimal.h"
#include "DefenseTurretBase.h"
#include "MainTurret.generated.h"

class ALaserProjectile;

UCLASS()
class SPACEACE_API AMainTurret : public ADefenseTurretBase
{
	GENERATED_BODY()

protected:
	virtual void FireAtTarget(AShipBase* Target) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	TSubclassOf<ALaserProjectile> ProjectileClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0.0"))
	float ProjectileSpeed = 50000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0.0"))
	float ProjectileDamage = 250.0f;
};
