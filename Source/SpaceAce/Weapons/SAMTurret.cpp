#include "SAMTurret.h"

#include "ShipBase.h"
#include "MissileProjectile.h"
#include "Components/SceneComponent.h"

void ASAMTurret::FireAtTarget(AShipBase* Target)
{
    if (!ProjectileClass || !Target) return;

    const USceneComponent* Muzzle = bFireFromPrimaryMuzzleNext
        ? GetPrimaryMuzzle()
        : GetSecondaryMuzzle();
    if (!Muzzle) return;

    const FVector Location = Muzzle->GetComponentLocation();
    const FRotator Rotation = Muzzle->GetComponentRotation();
    AMissileProjectile* Projectile = GetWorld()->SpawnActor<AMissileProjectile>(
        ProjectileClass, Location, Rotation);
    if (!Projectile) return;

    Projectile->SetLifeSpan(ProjectileLifetime+1.f);
    Projectile->InitializeProjectile(ProjectileSpeed, ProjectileDamage, ProjectileLifetime);
    Projectile->ConfigureFlight(true, FVector::ZeroVector, 0.0f);
    Projectile->ActivateProjectile(Location, Rotation, this, ProjectileDamage);
    Projectile->SetTarget(Target);
    bFireFromPrimaryMuzzleNext = !bFireFromPrimaryMuzzleNext;
}
