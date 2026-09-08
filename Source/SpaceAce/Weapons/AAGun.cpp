#include "AAGun.h"

#include "ShipBase.h"
#include "LaserProjectile.h"
#include "Components/SceneComponent.h"

void AAAGun::FireAtTarget(AShipBase* Target)
{
    if (!ProjectileClass || !Target) return;

    const USceneComponent* Muzzles[] = { GetPrimaryMuzzle(), GetSecondaryMuzzle() };
    for (const USceneComponent* Muzzle : Muzzles)
    {
        if (!Muzzle) continue;
        const FVector Location = Muzzle->GetComponentLocation();
        const FRotator Rotation = Muzzle->GetComponentRotation();
        ALaserProjectile* Projectile = GetWorld()->SpawnActor<ALaserProjectile>(
            ProjectileClass, Location, Rotation);
        if (Projectile)
        {
            Projectile->ActivateProjectile(Location, Rotation, this,
                ProjectileDamage, ProjectileSpeed);
        }
    }
}
